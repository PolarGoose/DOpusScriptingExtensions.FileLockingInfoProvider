#pragma once

#include <grpc_generated/FileLockingInfoProvider.grpc.pb.h>
#include "FileLockingInfoProvider/FileLockingInfoProvider/ProcessInfo.h"

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
    _connection = GrpcGenerated::FileLockingInfoProviderServiceGrpc::NewStub(grpc::CreateChannel("127.0.0.1:43786", grpc::InsecureChannelCredentials()));
    _updateThread = std::thread(&LockedFilesDatabase::UpdateDatabaseThread, this);
  }

  void UpdateDatabaseThread() {
    SPDLOG_INFO("Start UpdateDatabaseThread");

    try {
      while (!_stopEvent.HasBeenNotified()) {
        spdlog::stopwatch sw;
        SPDLOG_INFO("Update locked files database");

        GrpcGenerated::LockingProcessInfos response;
        grpc::ClientContext context;
        const auto& res = _connection->GetLockingProcessInfos(&context, google::protobuf::Empty(), &response);
        if(!res.ok()) {
          SPDLOG_ERROR("GRPC call `GetLockingProcessInfos` failed:\n{}", res.error_message());
        }

        std::map<std::wstring /* lockedFilePath */, std::set<uint64_t> /* pids */> lockedFilesMap;
        std::map<uint64_t /* pid */, ATL::CComPtr<IProcessInfo>> processInfosMap;

        for (const auto& [pid, processInfo] : response.process_infos()) {
          if (!processInfosMap.contains(pid)) {
            processInfosMap[pid] = CreateComObject<CProcessInfo, IProcessInfo>(
              [&](auto& obj) {
                obj.Init(
                  static_cast<UINT>(pid),
                  processInfo.has_executable_path() ? ToUtf16(processInfo.executable_path()) : L"",
                  processInfo.has_domain_name() ? ToUtf16(processInfo.domain_name()) : L"",
                  processInfo.has_user_name() ? ToUtf16(processInfo.user_name()) : L"");
              });
          }

          for (const auto& lockedFilePath : processInfo.locked_files()) {
            lockedFilesMap[boost::algorithm::to_lower_copy(ToUtf16(lockedFilePath))].emplace(pid);
          }

          for (const auto& lockedModulePath : processInfo.modules()) {
            lockedFilesMap[boost::algorithm::to_lower_copy(ToUtf16(lockedModulePath))].emplace(pid);
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
  static void NormalizeFilePath(std::wstring& path) {
    path = std::filesystem::canonical(path);
    if (std::filesystem::is_directory(path) && !path.ends_with(L'\\')) {
      path += L'\\';
    }
    boost::algorithm::to_lower(path);
  }

  mutable std::mutex _databaseMutex;
  std::map<std::wstring /* lockedFilePath */, std::set<uint64_t> /* pids */> _lockedFilesMap;
  std::map<uint64_t /* pid */, ATL::CComPtr<IProcessInfo>> _processInfosMap;
  std::thread _updateThread;
  absl::Notification _stopEvent;
  std::unique_ptr<GrpcGenerated::FileLockingInfoProviderServiceGrpc::Stub> _connection;
};
