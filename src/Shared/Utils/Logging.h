#pragma once

inline void ConfigureGlobalSpdLogger(const std::filesystem::path& logFileFullName) {
  static std::once_flag flag;
  std::call_once(flag, [&] {
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e][%s:%!:%#][thread %t] %v");
    spdlog::set_default_logger(spdlog::rotating_logger_mt(
      /* logger_name   */ "Global SPD logger",
      /* filename      */ logFileFullName.string(),
      /* max_file_size */ 10 * 1024 * 1024, /* 10 MiB */
      /* max_files     */ 1));
    spdlog::flush_every(std::chrono::seconds(5));
  });
};
