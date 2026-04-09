#pragma once

#include <grpc_generated/FileLockingInfoProvider.grpc.pb.h>
#include "Shared/GrpcAddress.h"
#include "FileLockingInfoProvider.ComDll/FileLockingInfoProvider/ProcessInfo.h"
#include "FileLockingInfoProvider.ComDll/FileLockingInfoProvider/DevicePathToDrivePathConverter.h"

class LockedFilesDatabase final : private boost::noncopyable {
public:
  static LockedFilesDatabase* GetInstance() {
    static LockedFilesDatabase instance;
    return &instance;
  }

  std::vector<ATL::CComPtr<IProcessInfo>> GetLockingProcesses(std::wstring path) const {
    NormalizeFilePath(path);

    std::lock_guard lock(_databaseMutex);

    std::set<uint64_t> uniquePids;
    if (!path.ends_with(L'\\')) {
      const auto fileIt = _lockedFilesMap.find(path);
      if (fileIt == _lockedFilesMap.end()) {
        return {};
      }
      uniquePids.insert_range(fileIt->second);
    }
    else {
      for (auto it = _lockedFilesMap.lower_bound(path); it != _lockedFilesMap.end() && it->first.starts_with(path); ++it) {
        uniquePids.insert_range(it->second);
      }
    }

    std::vector<ATL::CComPtr<IProcessInfo>> result;
    result.reserve(uniquePids.size());
    for (uint64_t pid : uniquePids) {
      if (auto it = _processInfosMap.find(pid); it != _processInfosMap.end()) {
        result.push_back(it->second);
      }
    }

    return result;
  }

  ~LockedFilesDatabase() {
    _stopEvent.Notify();
    _updateThread.join();
  }

private:
  LockedFilesDatabase() {
    _connection = GrpcGenerated::FileLockingInfoProviderServiceGrpc::NewStub(grpc::CreateChannel(GetGrpcUnixSocketAddress(), grpc::InsecureChannelCredentials()));
    _updateThread = std::thread(&LockedFilesDatabase::UpdateDatabaseThread, this);
  }

  void UpdateDatabaseThread() {
    SPDLOG_INFO("Start UpdateDatabaseThread");

    try {
      while (!_stopEvent.HasBeenNotified()) {
        spdlog::stopwatch sw;
        SPDLOG_INFO("Update locked files database");

        SPDLOG_INFO("Create DevicePathToDrivePathConverter");
        DevicePathToDrivePathConverter devicePathToDrivePathConverter;

        SPDLOG_INFO("Call _connection->GetLockingProcessInfos");
        GrpcGenerated::LockingProcessInfos response;
        grpc::ClientContext context;
        const auto& res = _connection->GetLockingProcessInfos(&context, google::protobuf::Empty(), &response);
        if (!res.ok()) {
          SPDLOG_ERROR("GRPC call `GetLockingProcessInfos` failed:\n{}", res.error_message());
        }
        SPDLOG_INFO("_connection->GetLockingProcessInfos finished. Took {} seconds", sw);

        std::map<std::wstring /* lockedFilePath */, std::set<uint64_t> /* pids */> lockedFilesMap;
        std::map<uint64_t /* pid */, ATL::CComPtr<IProcessInfo>> processInfosMap;

        for (const auto& [pid, processInfo] : response.process_infos()) {
          processInfosMap[pid] = CreateComObject<CProcessInfo, IProcessInfo>(
            [&](auto& obj) {
              obj.Init(
                static_cast<UINT>(pid),
                processInfo.has_executable_path() ? ToUtf16(processInfo.executable_path()) : L"",
                processInfo.has_domain_name() ? ToUtf16(processInfo.domain_name()) : L"",
                processInfo.has_user_name() ? ToUtf16(processInfo.user_name()) : L"");
            });

          for (const auto& lockedPath : boost::range::join(processInfo.locked_files(), processInfo.modules())) {
            const auto lockedPathStr = ToUtf16(lockedPath);

            // Network file or folder like "\Device\Mup\wsl.localhost\Ubuntu-24.04\home\user"
            // Convert it to UNC path like "\\wsl.localhost\Ubuntu-24.04\home\user\"
            if (lockedPathStr.starts_with(L"\\Device\\Mup\\")) {
              auto uncPath = std::format(L"\\{}", lockedPathStr.substr(11));
              AddTrailingBackslashIfDirectory(uncPath);
              lockedFilesMap[boost::algorithm::to_lower_copy(uncPath)].emplace(pid);
              continue;
            }

            if(lockedPath.starts_with("\\Device\\")) {
              auto driveBasedPath = devicePathToDrivePathConverter.GetDriveLetterBasedFullName(lockedPathStr);
              if (driveBasedPath) {
                AddTrailingBackslashIfDirectory(*driveBasedPath);
                lockedFilesMap[boost::algorithm::to_lower_copy(*driveBasedPath)].emplace(pid);
              }
              continue;
            }

            // Non device based path are modules, they are never directories. Thus we don't need to call `AddTrailingBackslashIfDirectory` for them.
            lockedFilesMap[boost::algorithm::to_lower_copy(ToUtf16(lockedPath))].emplace(pid);
          }
        }

        {
          std::lock_guard lock(_databaseMutex);
          _lockedFilesMap = std::move(lockedFilesMap);
          _processInfosMap = std::move(processInfosMap);
        }

        SPDLOG_INFO("Update database finished. Took {} seconds", sw);
        _stopEvent.WaitForNotificationWithTimeout(absl::Seconds(5));
      }
    }
    catch (const std::exception& ex) {
      SPDLOG_ERROR("Exit UpdateDatabaseThread because of exception:\n{}", ex.what());
    }
  }

  // Examples:
  // * "C:\Test\File.txt" -> "c:\test\file.txt"
  // * "C:/Test/File.txt" -> "c:\test\file.txt"
  // * "C:\Test\Directory" -> "c:\test\directory\"
  // * "C:\Test\Directory\\" -> "c:\test\directory\"
  // * "C:\Test\Directory\" -> "c:\test\directory\"
  // * "Z:\home\user" -> "\\wsl.localhost\ubuntu-24.04\home\user\"
  static void NormalizeFilePath(std::wstring& path) {
    std::error_code ec;
    auto normalizedPath = std::filesystem::canonical(path, ec);
    path = ec ? std::filesystem::absolute(path).lexically_normal() : std::move(normalizedPath);
    if (std::filesystem::is_directory(path, ec) && !path.ends_with(L'\\')) {
      path.push_back(L'\\');
    }
    boost::algorithm::to_lower(path);
  }

  static void AddTrailingBackslashIfDirectory(std::wstring& path) {
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec) && !path.ends_with(L'\\')) {
      path.push_back(L'\\');
    }
  }

  mutable std::mutex _databaseMutex;
  std::map<std::wstring /* lockedFilePath */, std::set<uint64_t> /* pids */> _lockedFilesMap;
  std::map<uint64_t /* pid */, ATL::CComPtr<IProcessInfo>> _processInfosMap;
  std::thread _updateThread;
  absl::Notification _stopEvent;
  std::unique_ptr<GrpcGenerated::FileLockingInfoProviderServiceGrpc::Stub> _connection;
};
