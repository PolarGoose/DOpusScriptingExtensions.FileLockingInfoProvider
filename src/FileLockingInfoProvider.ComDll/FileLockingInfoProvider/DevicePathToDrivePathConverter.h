#pragma once

#include "Shared/Utils/ScopedHandle.h"

class DevicePathToDrivePathConverter final : boost::noncopyable {
  std::map<std::wstring /* DeviceName */, std::wstring /* MountPath */> _deviceNameToMountPathMap;

  static bool StartsWithCaseInsensitive(const std::wstring_view str, const std::wstring_view prefix) {
    if (str.size() < prefix.size()) return false;
    return _wcsnicmp(str.data(), prefix.data(), prefix.size()) == 0;
  }

public:
  // Creates a conversion map {DeviceName, MountPath} from two sources:
  //
  // 1. DOS devices (QueryDosDevice): maps drive letters to their device names.
  // Example:
  //   [ {"\Device\HardDiskVolume2\"                                 , "C:\"},
  //     {"\Device\HardDiskVolume15\"                                , "D:\"},
  //     {"\Device\VBoxMiniRdr\;H:\VBoxSvr\My-H\"                    , "H:\"},
  //     {"\Device\LanmanRedirector\;I:000215d7\10.22.3.84\i\"       , "I:\"},
  //     {"\Device\CdRom0\"                                          , "X:\"} ]
  //
  // 2. Mounted volumes (FindFirstVolume/FindNextVolume): maps volume GUID paths to mount points.
  // Example:
  //   [ {"\Device\Volume{470e4501-335c-11f1-b2d8-2c0da7fb0517}\", "E:\"},
  //     {"\Device\Volume{a1b2c3d4-5678-90ab-cdef-1234567890ab}\", "C:\Mount\"} ]
  DevicePathToDrivePathConverter() {
    PopulateDosDevices();
    PopulateMountedVolumes();
  }

  // Converts device-based path to a drive/mount-point-based path.
  // Examples:
  //   "\Device\HardDiskVolume3\Windows\System32\en-US\KernelBase.dll.mui" -> "C:\Windows\System32\en-US\KernelBase.dll.mui"
  //   "\Device\Volume{470e4501-335c-11f1-b2d8-2c0da7fb0517}\test_doc.docx" -> "E:\test_doc.docx"
  std::optional<std::wstring> GetDriveLetterBasedFullName(const std::wstring_view deviceBasedFileFullName) const {
    for (const auto& [deviceName, mountPath] : _deviceNameToMountPathMap) {
      if (StartsWithCaseInsensitive(deviceBasedFileFullName, deviceName)) {
        return std::format(L"{}{}", mountPath, deviceBasedFileFullName.substr(deviceName.length()));
      }
    }
    return {};
  }

private:
  void PopulateDosDevices() {
    for (auto driveLetter = L'A'; driveLetter <= L'Z'; driveLetter++) {
      std::array<wchar_t, 10000> deviceNameBuf;
      const wchar_t drive[]{ driveLetter, L':', 0 };

      const auto length = QueryDosDevice(/* lpDeviceName */ drive,
                                         /* lpTargetPath */ deviceNameBuf.data(),
                                         /* ucchMax      */ static_cast<DWORD>(deviceNameBuf.size()));
      if (length == 0) {
        continue;
      }

      // The returned from QueryDosDevice device name doesn't have an '\' at the end.
      // We add it to be able to distinguish "\Device\HardDiskVolume1\path" from "\Device\HardDiskVolume10\path"
      // when converting device names to drive letters.
      _deviceNameToMountPathMap.emplace(std::wstring{ deviceNameBuf.data() } + L'\\',
                                        std::wstring{ driveLetter, L':', L'\\' });
    }
  }

  void PopulateMountedVolumes() {
    std::array<wchar_t, 10000> volumeNameBuf;

    ScopedFindVolumeHandle findHandle{ FindFirstVolumeW(volumeNameBuf.data(), static_cast<DWORD>(volumeNameBuf.size())) };
    if (findHandle.get() == INVALID_HANDLE_VALUE) {
      return;
    }

    do {
      // volumeNameBuf contains something like "\\?\Volume{GUID}\"
      std::wstring volumeName{ volumeNameBuf.data() };

      auto mountPath = GetMountPathForVolume(volumeName);
      if (!mountPath) {
        continue;
      }

      _deviceNameToMountPathMap.emplace(L"\\Device\\" + volumeName.substr(4), std::move(*mountPath));
    } while (FindNextVolumeW(findHandle.get(), volumeNameBuf.data(), static_cast<DWORD>(volumeNameBuf.size())));
  }

  static std::optional<std::wstring> GetMountPathForVolume(const std::wstring& volumeName) {
    DWORD charCount = 0;

    GetVolumePathNamesForVolumeNameW(/* lpszVolumeName      */ volumeName.c_str(),
                                     /* lpszVolumePathNames */ nullptr,
                                     /* cchBufferLength     */ 0,
                                     /* lpcchReturnLength   */ &charCount);
    if (charCount == 0) {
      return {};
    }

    auto pathNamesBuf = std::make_unique_for_overwrite<wchar_t[]>(charCount);
    if (!GetVolumePathNamesForVolumeNameW(/* lpszVolumeName      */ volumeName.c_str(),
                                          /* lpszVolumePathNames */ pathNamesBuf.get(),
                                          /* cchBufferLength     */ charCount,
                                          /* lpcchReturnLength   */ &charCount)) {
      return {};
    }

    std::wstring firstPath{ pathNamesBuf.get() };
    if (firstPath.empty()) {
      return {};
    }

    return firstPath;
  }
};
