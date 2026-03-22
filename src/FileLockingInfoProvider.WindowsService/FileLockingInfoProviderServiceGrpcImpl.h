#pragma once
#include <grpc_generated/FileLockingInfoProvider.pb.h>
#include <grpc_generated/FileLockingInfoProvider.grpc.pb.h>
#include "FileLockingInfoProvider.WindowsService/LockedFilesProvider.h"

class FileLockingInfoProviderServiceGrpcImpl final : public GrpcGenerated::FileLockingInfoProviderServiceGrpc::Service {
  LockedFilesProvider _lockedFilesProvider;

public:
  grpc::Status GetLockingProcessInfos(grpc::ServerContext* /* context */,
                                      const google::protobuf::Empty* /* request */,
                                      GrpcGenerated::LockingProcessInfos* response) override {
    try {
      spdlog::stopwatch sw;
      SPDLOG_INFO("GetLockingProcessInfos Start");
      _lockedFilesProvider.GetLockingProcessInfos(*response);
      SPDLOG_INFO("GetLockingProcessInfos Finished. Took {} seconds", sw);
      return grpc::Status::OK;
    }
    catch (const std::exception& ex) {
      SPDLOG_ERROR("GetLockingProcessInfos Failed: {}", ex.what());
      return grpc::Status(grpc::StatusCode::INTERNAL, ex.what());
    }
  }
};
