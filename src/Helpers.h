#pragma once

namespace fs = std::filesystem;

_NODISCARD inline bool IsGameThread() noexcept {
  const auto* main = RE::Main::GetSingleton();
  return main && main->threadID == REX::W32::GetCurrentThreadId();
}

inline void trim_end(std::string& str) {
  while (!str.empty() && str.back() == '\0') {
    str.pop_back();
  }
}

#define FIND_IN(_ITERATOR, _FIND) \
  if (const auto it = _ITERATOR.find(_FIND); it != _ITERATOR.end())

#define IGNORE(_RESULT) (void)_RESULT