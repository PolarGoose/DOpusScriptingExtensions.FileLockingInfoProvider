A COM DLL that provides `FileLockingInfoProvider` COM class that can be used by JScript scripts for [Directory Opus](https://www.gpsoft.com.au/).<br>
This class provides information about processes that are locking a specified file or folder.<br>
This is a standalone project. It doesn't require [DOpus-Scripting-Extensions](https://github.com/PolarGoose/DOpus-Scripting-Extensions) to be installed.

# Limitations
* Supports Windows 10 x64 or higher.
* Supports only the JScript scripting language. VBScript is not supported.
* Portable Directory Opus is not supported because the DLL requires installation using an MSI installer.

# How to use
* Download the installer from the [latest release](https://github.com/PolarGoose/DOpusScriptingExtensions.FileLockingInfoProvider/releases) and install it on your system
* After that, you can access the functionality from any JScript or DOpus script.

# Example
```js
// Create an instance of the FileLockingInfoProvider COM class.
// When you create the first instance, it starts the background loop that updates the file locking database every 5 seconds.
var fileLockingInfoProvider = new ActiveXObject("DOpusScriptingExtensions.FileLockingInfoProvider")

// GetLockingProcesses retrieves the list of processes that are locking the specified folder or any file or folder inside it.
// The list of processes includes processes from all users, including Admin and System.
// It retrieves information from the cache, which is updated in the background every 5 seconds.
// Thus, it is very fast (takes about 1-2 milliseconds on my machine).
// It returns a SAFEARRAY, you can convert it to a JavaScript array.
// As a parameter, you can specify a folder or a file path.
var processInfos = new VBArray(fileLockingInfoProvider.GetLockingProcesses("C:/Windows")).toArray()
processInfos = new VBArray(fileLockingInfoProvider.GetLockingProcesses("C:\\Windows\\System32\\en-US\\svchost.exe.mui")).toArray()

for (var i = 0; i < processInfos.length; i++) {
  var info = processInfos[i]
  WScript.Echo("Pid: " + info.Pid) // Process ID
  WScript.Echo("ExecutablePath: " + info.ExecutablePath) // Executable full name, f.e. "C:\Windows\System32\svchost.exe"
  WScript.Echo("DomainName: " + info.DomainName) // Domain name of the user that is running the process, f.e, "NT AUTHORITY"
  WScript.Echo("UserName: " + info.UserName) // User name of the user that is running the process, f.e, "UserName"
}
```

# How it works
The project consists of two parts:
* Windows Service `DOpusScriptingExtensions.FileLockingInfoProvider.WindowsService.exe`
  * Runs automatically when Windows starts.
  * Uses `PROCEXP152.SYS` driver to get information about all locked files and folders in the system and the processes that lock them.
  * Also takes into account loaded DLLs and memory-mapped files.
  * Uses [Google RPC](https://grpc.io/) to allow the Directory Opus process to access the service functionality.
  * Running as a service allows getting information about all processes, including running as an Admin or System. For security reasons, only the list of processes and locked files is exposed. No other information can be requested from the service.
* COM DLL `DOpusScriptingExtensions.FileLockingInfoProvider.dll`
  * Gets loaded by Directory Opus when `new ActiveXObject("DOpusScriptingExtensions.FileLockingInfoProvider")` is called.
  * Runs a background loop that updates the file locking database every 5 seconds. It calls `DOpusScriptingExtensions.FileLockingInfoProvider.WindowsService` Google RPC interface to get the list of processes and their locked files.

# How to build
## Development environment
* `Visual Studio 2026` with [HeatWave for VS2022](https://marketplace.visualstudio.com/items?itemName=FireGiant.FireGiantHeatWaveDev17) plugin.
* Open `DOpusScriptingExtensions.FileLockingInfoProvider.sln` using `Visual Studio 2026` as an admin.

## Build instructions
* Run `build.ps1` script as an admin
