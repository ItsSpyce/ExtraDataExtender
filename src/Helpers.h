#pragma once

namespace fs = std::filesystem;

_NODISCARD inline bool IsGameThread() noexcept {
  const auto* main = RE::Main::GetSingleton();
  return main && main->threadID == REX::W32::GetCurrentThreadId();
}
