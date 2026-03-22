#pragma once

inline void ConfigureGlobalSpdLogger(const std::wstring_view logFileName) {
  static std::once_flag flag;
  std::call_once(flag, [&] {
    auto logFilePath = std::filesystem::temp_directory_path() / logFileName;

    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e][%s:%!:%#][thread %t] %v");
    spdlog::set_default_logger(spdlog::rotating_logger_mt(
      /* logger_name   */ "Global SPD logger",
      /* filename      */ logFilePath.string(),
      /* max_file_size */ 2 * 1024 * 1024, /* 2 MiB */
      /* max_files     */ 2));
    spdlog::flush_every(std::chrono::seconds(2));
  });
};
