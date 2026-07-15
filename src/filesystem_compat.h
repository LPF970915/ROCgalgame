#pragma once

#if __has_include(<filesystem>) && (!defined(__GNUC__) || __GNUC__ >= 8)
#include <filesystem>
#else
#include <experimental/filesystem>
namespace std {
namespace filesystem = experimental::filesystem;
}
#endif

#include <system_error>

namespace filesystem_compat {

inline std::filesystem::path LexicallyNormal(const std::filesystem::path &path) {
#if defined(__GNUC__) && __GNUC__ < 8
  return path;
#else
  return path.lexically_normal();
#endif
}

inline std::filesystem::path WeaklyCanonical(const std::filesystem::path &path, std::error_code &ec) {
#if defined(__GNUC__) && __GNUC__ < 8
  ec.clear();
  return path;
#else
  return std::filesystem::weakly_canonical(path, ec);
#endif
}

inline std::filesystem::path LexicallyRelative(const std::filesystem::path &path,
                                               const std::filesystem::path &base) {
#if defined(__GNUC__) && __GNUC__ < 8
  auto pit = path.begin();
  auto bit = base.begin();
  for (; bit != base.end() && pit != path.end() && *bit == *pit; ++bit, ++pit) {
  }
  if (bit != base.end()) return {};
  std::filesystem::path out;
  for (; pit != path.end(); ++pit) out /= *pit;
  return out;
#else
  return path.lexically_relative(base);
#endif
}

inline bool IsRegularFile(const std::filesystem::directory_entry &entry, std::error_code &ec) {
  return std::filesystem::is_regular_file(entry.path(), ec);
}

inline bool IsRegularFile(const std::filesystem::directory_entry &entry) {
  std::error_code ec;
  return IsRegularFile(entry, ec) && !ec;
}

inline bool IsDirectory(const std::filesystem::directory_entry &entry, std::error_code &ec) {
  return std::filesystem::is_directory(entry.path(), ec);
}

inline bool IsDirectory(const std::filesystem::directory_entry &entry) {
  std::error_code ec;
  return IsDirectory(entry, ec) && !ec;
}

inline std::filesystem::file_time_type LastWriteTime(const std::filesystem::directory_entry &entry,
                                                     std::error_code &ec) {
  return std::filesystem::last_write_time(entry.path(), ec);
}

} // namespace filesystem_compat
