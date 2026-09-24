#pragma once

#include <fmt/format.h>

#include <future>

template <class T>
using Singleton = REX::Singleton<T>;

namespace stl {
using namespace SKSE::stl;

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
  auto enumRange =
      std::views::iota(std::to_underlying(first), std::to_underlying(last)) |
      std::views::transform(
          [](auto enumVal) { return (decltype(first))enumVal; });
  return enumRange;
}
}  // namespace stl

template <typename T>
struct Ok {
  T value;

  template <typename U>
  Ok(U&& v)
    requires(std::is_constructible_v<T, U &&>)
      : value(std::forward<U>(v)) {}
};

template <std::size_t N>
Ok(const char (&)[N]) -> Ok<std::string>;

template <typename T>
Ok(T&&) -> Ok<std::decay_t<T>>;

#ifdef FMT_VERSION
template <typename T>
struct fmt::formatter<Ok<T>> : formatter<string_view> {
  auto format(const Ok<T>& ok, format_context& ctx) const {
    return formatter<string_view>::format(fmt::format("{}", ok.value), ctx);
  }
};
#endif

struct Err {
  std::string message;
  std::int32_t code{0};

  Err(std::string message) : message(std::move(message)) {}
  Err(const char* message) : message(message) {}
  template <typename... Args>
  Err(fmt::format_string<Args...> format, Args&&... args)
      : message(fmt::format(format, std::forward<Args>(args)...)) {}

  Err(std::int32_t code, std::string message)
      : message(std::move(message)), code(code) {}
  Err(std::int32_t code, const char* message) : message(message), code(code) {}
  template <typename... Args>
  Err(std::int32_t code, fmt::format_string<Args...> format, Args&&... args)
      : message(fmt::format(format, std::forward<Args>(args)...)), code(code) {}

  Err(const std::optional<Err>& rhs) : message(rhs->message), code(rhs->code) {}
  Err(std::optional<Err>& rhs) : message(rhs->message), code(rhs->code) {}

  auto operator<=>(const Err&) const = default;
  bool operator==(const Err& rhs) const { return message == rhs.message; }
  bool operator!=(const Err& rhs) const { return message != rhs.message; }
};

#ifdef FMT_VERSION
template <>
struct fmt::formatter<Err> : formatter<string_view> {
  auto format(const Err& err, format_context& ctx) const {
    return formatter<string_view>::format(err.message, ctx);
  }
};
#endif

template <typename T>
class _NODISCARD result {
 protected:
  std::variant<T, Err> data_;

 public:
  using value_type = T;
  using value_p = T*;
  using value_lval = T&;
  using value_rval = T&&;
  using error_type = std::string;

  template <typename U>
  result(Ok<U>&& ok)
    requires(std::is_constructible_v<T, U &&>)
      : data_(std::in_place_index<0>, std::forward<U>(ok.value)) {}
  template <typename U>
  result(const Ok<U>& ok)
    requires(std::is_constructible_v<T, const U&>)
      : data_(std::in_place_index<0>, ok.value) {}
  result(Err&& err) : data_(std::in_place_index<1>, std::move(err)) {}
  result(const Err& err) : data_(std::in_place_index<1>, err) {}

  value_p value_ptr() noexcept { return std::get_if<0>(&data_); }

  const value_p value_ptr() const noexcept { return std::get_if<0>(&data_); }

  Err* error_ptr() noexcept { return std::get_if<1>(&data_); }

  const Err* error_ptr() const noexcept { return std::get_if<1>(&data_); }

  template <std::size_t I>
  decltype(auto) get() & noexcept {
    static_assert(I < 2);
    if constexpr (I == 0) {
      return value_ptr();
    } else {
      return error_ptr();
    }
  }

  template <std::size_t I>
  decltype(auto) get() const& noexcept {
    static_assert(I < 2);
    if constexpr (I == 0) {
      return static_cast<const T*>(value_ptr());
    } else {
      return static_cast<const Err*>(error_ptr());
    }
  }

  template <std::size_t I>
  decltype(auto) get() && noexcept {
    static_assert(I < 2);
    if constexpr (I == 0) {
      return value_ptr();
    } else {
      return error_ptr();
    }
  }

  bool is_ok() const noexcept { return data_.index() == 0; }

  bool is_err() const noexcept { return data_.index() == 1; }

  explicit operator bool() const noexcept { return is_ok(); }

  value_lval value() & { return std::get<0>(data_); }

  const value_lval& value() const& { return std::get<0>(data_); }

  value_rval&& value() && { return std::get<0>(std::move(data_)); }

  Err& error() & { return std::get<1>(data_); }

  const Err& error() const& { return std::get<1>(data_); }

  Err&& error() && { return std::get<1>(std::move(data_)); }

  void assert_ok() & { assert(is_ok()); }

  void assert_ok() && { assert(is_ok()); }

  template <typename _Match>
  auto match(const std::function<_Match(value_lval)>& ok,
             const std::function<_Match(const Err&)>& err) noexcept {
    if (is_ok()) {
      return ok(value());
    }
    return err(error());
  }

  template <typename _Match>
  auto match(const std::function<_Match(value_lval)>& ok,
             const std::function<_Match(const Err&)>& err) const noexcept {
    if (is_ok()) {
      return ok(value());
    }
    return err(error());
  }

  value_lval value_or(T other) const & {
    if (is_ok()) {
      return value();
    }
    return other;
  }

  value_rval value_or(T other) const && {
    if (is_ok()) {
      return value();
    }
    return other;
  }

  auto operator->() const { return value(); }

  auto operator->() { return value(); }

  auto into_options() && {
    using options_type = std::pair<std::optional<T>, std::optional<Err>>;
    if (is_ok()) {
      return options_type{
          std::optional<T>{std::move(*this).value()},
          std::nullopt,
      };
    }
    return options_type{std::nullopt,
                        std::optional<Err>{std::move(*this).error()}};
  }
};

#ifdef FMT_VERSION
template <typename T>
struct fmt::formatter<result<T>> : formatter<string_view> {
  auto format(const result<T>& r, format_context& ctx) const {
    if (r.is_ok()) {
      return formatter<string_view>::format(fmt::format("{}", r.value()), ctx);
    } else {
      return formatter<string_view>::format(r.error(), ctx);
    }
  }
};
#endif

template <typename T>
struct std::tuple_size<result<T>> : std::integral_constant<size_t, 2> {};
template <typename T>
struct std::tuple_element<0, result<T>> {
  using type = const T*;
};
template <typename T>
struct std::tuple_element<1, result<T>> {
  using type = const Err*;
};

template <typename T>
using fresult = std::future<result<T>>;

template <typename T, typename Returns = std::invoke_result_t<T>>
  requires std::is_invocable_v<T>
result<Returns> try_catch(const T& fn) noexcept {
  try {
    return Ok{fn()};
  } catch (const std::runtime_error& error) {
    return Err{error.what()};
  }
}
