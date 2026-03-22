#include "pch.h"
#include <doctest/doctest.h>
#include "FileLockingInfoProvider.WindowsService/FileLockingInfoProviderServiceGrpcImpl.h"

static void AssertContainsProcess(const GrpcGenerated::LockingProcessInfos& lockingProcessInfos,
                                  std::function<bool(const GrpcGenerated::ProcessInfo&)> predicate,
                                  const std::string_view errorMessage) {
  for (const auto& [pid, processInfo] : lockingProcessInfos.process_infos()) {
    if (predicate(processInfo)) {
      return;
    }
  }
  FAIL(errorMessage);
}

static void AssertDoesNotContainProcess(const GrpcGenerated::LockingProcessInfos& lockingProcessInfos,
                                        std::function<bool(const GrpcGenerated::ProcessInfo&)> predicate,
                                        const std::string_view errorMessage) {
  for (const auto& [pid, processInfo] : lockingProcessInfos.process_infos()) {
    if (!predicate(processInfo)) {
      return;
    }
  }
  FAIL(errorMessage);
}

static bool ContainsElement(const google::protobuf::RepeatedPtrField<std::string>& array, const std::string& element) {
  for (const auto& item : array) {
    if (_stricmp(item.c_str(), element.c_str()) == 0) {
      return true;
    }
  }
  return false;
}

static bool ContainsElement(const google::protobuf::RepeatedPtrField<std::string>& array, std::function<bool(const std::string&)> predicate) {
  for (const auto& item : array) {
    if (predicate(item)) {
      return true;
    }
  }
  return false;
}

TEST_CASE("ProcessHandlesServiceGrpcServer test") {
  FileLockingInfoProviderServiceGrpcImpl processHandlesServiceGrpcImpl;

  grpc::ServerBuilder builder;
  int selectedPort = 0;
  builder.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials(), &selectedPort);
  builder.RegisterService(&processHandlesServiceGrpcImpl);
  const auto grpcServer = builder.BuildAndStart();

  auto client = GrpcGenerated::FileLockingInfoProviderServiceGrpc::NewStub(grpc::CreateChannel("127.0.0.1:" + std::to_string(selectedPort), grpc::InsecureChannelCredentials()));

  GrpcGenerated::LockingProcessInfos response;
  grpc::ClientContext context;
  auto start = std::chrono::high_resolution_clock::now();
  const auto& status = client->GetLockingProcessInfos(&context, google::protobuf::Empty(), &response);
  std::cout << std::format("GetLockingProcessInfos time taken: {}ms\n", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count());

  REQUIRE(status.ok());
  REQUIRE(response.process_infos_size() > 0);
  REQUIRE(response.process_infos().contains(4)); // Contains "System" process

  AssertContainsProcess(response,
    [](const auto& info) { return info.domain_name() == "NT AUTHORITY"; },
    "Should have process with domain_name = 'NT AUTHORITY'");

  AssertContainsProcess(response,
    [](const auto& info) { return info.user_name() == "SYSTEM"; },
    "Should have process with user_name = 'SYSTEM'");

  AssertContainsProcess(response,
    [](const auto& info) { return ContainsElement(info.modules(), "C:\\WINDOWS\\SYSTEM32\\ntdll.dll"); },
    "Should contain module 'C:\\WINDOWS\\SYSTEM32\\ntdll.dll'");

  AssertContainsProcess(response,
    [](const auto& info) { return ContainsElement(info.modules(), "C:\\Windows\\System32\\svchost.exe"); },
    "Should contain module 'C:\\Windows\\System32\\svchost.exe'");

  AssertContainsProcess(response,
    [](const auto& info) { return ContainsElement(info.locked_files(), "C:\\Windows\\System32\\en-US\\svchost.exe.mui"); },
    "Should contain locked file 'C:\\Windows\\System32\\en-US\\svchost.exe.mui'");

  AssertContainsProcess(response,
    [](const auto& info) { return ContainsElement(info.locked_files(), "C:\\Windows\\System32\\"); },
    "Should contain locked folder 'C:\\Windows\\System32\\'");

  AssertDoesNotContainProcess(response,
    [](const auto& info) { return ContainsElement(info.modules(), [&](const std::string& path) { return path.starts_with(R"(\\?\)"); }); },
    "Should not contain path that start with \\\\?\\");
}
