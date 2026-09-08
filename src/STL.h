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

template <typename T>
struct Ok {
  T value;

  template <typename U,
            std::enable_if_t<std::is_constructible_v<T, U&&>, int> = 0>
  Ok(U&& v) : value(std::forward<U>(v)) {}
};

template <std::size_t N>
Ok(const char (&)[N]) -> Ok<std::string>;

template <typename T>
Ok(T&&) -> Ok<std::decay_t<T>>;

#ifdef FMT_VERSION
template <typename T>
struct fmt::formatter<Ok<T>> : formatter<string_view> {
  auto format(Ok<T>& ok, format_context& ctx) const {
    return formatter<string_view>::format(ok.value(), ctx);
  }
};
#endif

struct Err {
  std::string error;

  Err(std::string message) : error(std::move(message)) {}
  Err(const char* message) : error(message) {}
  template <typename... Args>
  Err(fmt::format_string<Args...> format, Args&&... args)
      : error(fmt::format(format, std::forward<Args>(args)...)) {}
};

#ifdef FMT_VERSION
template <>
struct fmt::formatter<Err> : formatter<string_view> {
  auto format(Err& err, format_context& ctx) const {
    return formatter<string_view>::format(err.error, ctx);
  }
};
#endif

template <typename T>
class result {
  std::variant<T, std::string> data_;

 public:
  using value_type = T;
  using error_type = std::string;

  template <typename U,
            std::enable_if_t<std::is_constructible_v<T, U&&>, int> = 0>
  result(Ok<U>&& ok)
      : data_(std::in_place_index<0>, std::forward<U>(ok.value)) {}
  template <typename U,
            std::enable_if_t<std::is_constructible_v<T, const U&>, int> = 0>
  result(const Ok<U>& ok) : data_(std::in_place_index<0>, ok.value) {}
  result(Err&& err) : data_(std::in_place_index<1>, std::move(err.error)) {}
  result(const Err& err) : data_(std::in_place_index<1>, err.error) {}

  _NODISCARD bool is_ok() const noexcept { return data_.index() == 0; }

  _NODISCARD bool is_err() const noexcept { return data_.index() == 1; }

  explicit operator bool() const noexcept { return is_ok(); }

  T& value() & { return std::get<0>(data_); }

  const T& value() const& { return std::get<0>(data_); }

  T&& value() && { return std::get<0>(std::move(data_)); }

  std::string& error() & { return std::get<1>(data_); }

  const std::string& error() const& { return std::get<1>(data_); }

  std::string&& error() && { return std::get<1>(std::move(data_)); }

  template <typename _Match>
  auto match(const std::function<_Match(T)>& ok,
             const std::function<_Match(const std::string&)>& err) {
    if (is_ok()) {
      return ok(value());
    }
    return err(error());
  }
};

#ifdef FMT_VERSION
template <typename T>
struct fmt::formatter<result<T>> : formatter<string_view> {
  auto format(result<T>& r, format_context& ctx) const {
    if (r.is_ok()) {
      return formatter<string_view>::format(r.value(), ctx);
    } else {
      return formatter<string_view>::format(r.error(), ctx);
    }
  }
};
#endif