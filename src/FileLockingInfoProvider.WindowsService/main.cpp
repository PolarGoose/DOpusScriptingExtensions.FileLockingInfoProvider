#include "pch.h"
#include "Shared/Utils/Logging.h"
#include "Shared/GrpcAddress.h"
#include "FileLockingInfoProvider.WindowsService/FileLockingInfoProviderServiceGrpcImpl.h"

static const auto& g_serviceName = L"DOpusScriptingExtensions.FileLockingInfoProvider.WindowsService";
static absl::Notification g_stopEvent;

static void SetState(const SERVICE_STATUS_HANDLE serviceStatusHandle, const DWORD state, const DWORD win32ExitCode = NO_ERROR, const DWORD waitHintMs = 0) {
  SERVICE_STATUS st{ .dwServiceType = SERVICE_WIN32_OWN_PROCESS,
                     .dwCurrentState = state,
                     .dwControlsAccepted = (state == SERVICE_RUNNING) ? (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN) : (DWORD) 0,
                     .dwWin32ExitCode = win32ExitCode,
                     .dwServiceSpecificExitCode = 0,
                     .dwCheckPoint = 0,
                     .dwWaitHint = waitHintMs };
  SetServiceStatus(serviceStatusHandle, &st);
}

static DWORD WINAPI ServiceCtrlHandlerEx(const DWORD control, const DWORD /* eventType */, void* /* eventData */, void* /* context */) {
  switch (control) {
  case SERVICE_CONTROL_STOP:
  case SERVICE_CONTROL_SHUTDOWN:
    SPDLOG_INFO("stop requested");
    g_stopEvent.Notify();
    return NO_ERROR;
  default:
    return NO_ERROR;
  }
}

static void WINAPI ServiceMain(const DWORD /* argc */, LPWSTR* /* argv */) {
  SPDLOG_INFO("ServiceMain start");

  const auto& serviceStatusHandle = RegisterServiceCtrlHandlerExW(g_serviceName, ServiceCtrlHandlerEx, nullptr);
  if (!serviceStatusHandle) {
    SPDLOG_ERROR("Failed to call RegisterServiceCtrlHandlerExW");
    return;
  }

  SetState(serviceStatusHandle, SERVICE_START_PENDING, NO_ERROR, 30000);

  try {
    SPDLOG_INFO("Load and start PROCEXP152.SYS driver");
    FileLockingInfoProviderServiceGrpcImpl processHandlesServiceGrpc;
    SPDLOG_INFO("PROCEXP152.SYS driver loaded");

    const auto& grpcUnixSocketAddress = GetGrpcUnixSocketAddress();
    const auto& grpcUnixSocketFilePath = grpcUnixSocketAddress.substr(5); // strip "unix:" prefix

    SPDLOG_INFO("Remove existing socket file if exists: '{}'", grpcUnixSocketFilePath);
    std::filesystem::remove(grpcUnixSocketFilePath); // remove existing socket file if exists

    SPDLOG_INFO("Start GRPC server using UDS '{}'", grpcUnixSocketAddress);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(grpcUnixSocketAddress, grpc::InsecureServerCredentials());
    builder.RegisterService(&processHandlesServiceGrpc);
    auto grpcServer = builder.BuildAndStart();
    SPDLOG_INFO("GRPC server started");

    SPDLOG_INFO("Set socket file permissions to allow all users to connect");
    if (const auto err = SetNamedSecurityInfoA(/* pObjectName  */ const_cast<char*>(grpcUnixSocketFilePath.c_str()),
                                               /* ObjectType   */ SE_FILE_OBJECT,
                                               /* SecurityInfo */ DACL_SECURITY_INFORMATION,
                                               /* psidOwner    */ nullptr,
                                               /* psidGroup    */ nullptr,
                                               /* pDacl        */ nullptr, // null DACL = unrestricted access
                                               /* pSacl        */ nullptr); err != ERROR_SUCCESS) {
      THROW_WEXCEPTION(L"Failed to set socket file permissions using 'SetNamedSecurityInfoA'. Error {}", err);
    }

    SetState(serviceStatusHandle, SERVICE_RUNNING);

    SPDLOG_INFO("ServiceMain waiting for stop signal");
    g_stopEvent.WaitForNotification();

    SetState(serviceStatusHandle, SERVICE_STOP_PENDING, NO_ERROR, 30000);

    SPDLOG_INFO("ServiceMain stop signal received. Shut down GRPC server");
    grpcServer->Shutdown();
    SPDLOG_INFO("GRPC server is shut down");
  } catch (const std::exception& ex) {
    SPDLOG_ERROR("Exception during service initialization: {}", ex.what());
    SetState(serviceStatusHandle, SERVICE_STOPPED, ERROR_SERVICE_SPECIFIC_ERROR);
    return;
  }

  SetState(serviceStatusHandle, SERVICE_STOPPED);
}

int main() {
  ConfigureGlobalSpdLogger("C:/Program Files/DOpusScriptingExtensions.FileLockingInfoProvider/DOpusScriptingExtensions.FileLockingInfoProvider.WindowsService.log.txt");

  SPDLOG_INFO("main start");

  SERVICE_TABLE_ENTRYW table[] = { { const_cast<LPWSTR>(g_serviceName), ServiceMain },
                                   { nullptr, nullptr } };

  if (!StartServiceCtrlDispatcherW(table)) {
    const WinApiException ex(L"StartServiceCtrlDispatcherW", LINE_INFO, L"Failed to start the service thread");
    SPDLOG_INFO("{}", ex.what());
    return static_cast<int>(GetLastError());
  }

  SPDLOG_INFO("main finish");
  return 0;
}
