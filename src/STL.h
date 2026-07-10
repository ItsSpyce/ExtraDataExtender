#pragma once

#include <any>

namespace stl {
using namespace SKSE::stl;

template <class T>
typedef REX::Singleton<T> Singleton;

template <typename T>
struct type_tag {
  using type = T;
};

template <class T, size_t Size = 14>
void write_thunk_call() {
  SKSE::AllocTrampoline(Size);

  auto& trampoline = SKSE::GetTrampoline();
  T::func =
      trampoline.write_call<5>(T::rel.address() + T::offset.offset(), T::thunk);
}

template <class F, class T>
void write_vfunc() {
  REL::Relocation vtbl{F::VTABLE[0]};
  T::func = vtbl.write_vfunc(T::idx, T::thunk);
}

#ifdef DETOURS_VERSION
template <class F>
void write_detour() {
  DetourAttach(&(PVOID&)F::func, F::thunk);
}
#endif

constexpr auto enum_range(auto first, auto last) {
  auto enum_range =
      std::views::iota(std::to_underlying(first), std::to_underlying(last)) |
      std::views::transform(
          [](auto enum_val) { return (decltype(first))enum_val; });
  return enum_range;
}

template <typename T>
_NODISCARD std::optional<std::remove_cv_t<T>> sany_cast(const std::any& any) {
  try {
    return std::make_optional(std::any_cast<T>(any));
  } catch (const std::bad_any_cast& e) {
    return std::nullopt;
  }
}
}  // namespace stl