#pragma once

namespace fsp {
namespace fs = std::filesystem;

inline void rm_dir(const fs::path& path) noexcept {
  if (!path.empty()) fs::remove_all(path);
}

inline fs::path reinit_dir(const fs::path& path) noexcept {
  rm_dir(path);
  fs::create_directories(path);
  return path;
}

struct TempDir {
  fs::path path;
  explicit TempDir(fs::path path) : path(std::move(path)) {}
  ~TempDir() {
    rm_dir(path);
    path.clear();
  }
  DONOTMOVEITMOVEIT(TempDir);

  fs::path GetAndRelease() noexcept {
    auto val = std::move(path);
    Release();
    return val;
  }

  void Release() noexcept {
    path.clear();
  }
};

// using this for when, just by chance, the game crashes post-save but before
// SKSE calls SerializationInterface's save. Super rare edge case, haven't
// encountered, but safety is good I guess
inline result<bool> flush_file(const fs::path& path) {
  const auto file = CreateFileW(path.c_str(), GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return Err{"Received error on creating file for flush: {}", GetLastError()};
  }
  const auto didFlush = FlushFileBuffers(file) != 0;
  const auto error = GetLastError();
  CloseHandle(file);  // ignore the result
  if (!didFlush) {
    return Err{"Received error on flushing file: {}", error};
  }
  return Ok{true};
}
static result<bool> commit_file(const fs::path& source, const fs::path& dest) {
  if (!MoveFileExW(source.c_str(), dest.c_str(), MOVEFILE_WRITE_THROUGH)) {
    return Err{"Received error on file commit: {}", GetLastError()};
  }
  return Ok{true};
}

static result<bool> replace_file(const fs::path& source, const fs::path& dest,
                                 const fs::path& temp) {
  if (!fs::exists(dest)) {
    return commit_file(source, dest);
  }
  rm_dir(temp);
  TempDir backup{temp};
  fs::rename(dest, backup.path);
  try {
    return commit_file(source, dest);
  } catch (std::exception& commitErr) {
    try {
      rm_dir(dest);
      fs::rename(backup.path, dest);
    } catch (std::exception& backupErr) {
      return Err{"Failed to publish file and backup restore failed: {}",
                 backupErr.what()};
    }
    return Err{"Failed to publish file, backup restore success: {}",
               commitErr.what()};
  }
}
}  // namespace fsp