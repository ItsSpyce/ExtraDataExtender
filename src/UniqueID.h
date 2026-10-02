#pragma once

#define UID_NONE uid_t::limits::min

class uid_t {
 public:
  struct limits {
    static constexpr uint64_t min = 0;
    static constexpr uint64_t max = (uint64_t{1} << 48) - 1;
  };

  template <typename _Ty>
  static constexpr bool can_be_uid =
      std::is_integral_v<_Ty> && std::is_unsigned_v<_Ty> &&
      !std::is_same_v<std::remove_cv_t<_Ty>, bool>;

  uid_t() = default;
  template <class _Ty>
    requires can_be_uid<_Ty>
  constexpr uid_t(const _Ty other) noexcept {
    auto value = static_cast<uint64_t>(other);
    for (auto& byte : values_) {
      byte = static_cast<uint8_t>(value);
      value >>= 8;
    }
  }

  constexpr uid_t(const uint32_t owner, const uint16_t native) noexcept
      : uid_t((static_cast<uint64_t>(native) << 32) | owner) {}

  constexpr bool operator==(const unsigned char rhs) const noexcept {
    return is_equal(rhs);
  }
  constexpr bool operator==(const unsigned short rhs) const noexcept {
    return is_equal(rhs);
  }
  constexpr bool operator==(const unsigned int rhs) const noexcept {
    return is_equal(rhs);
  }
  constexpr bool operator==(const unsigned long rhs) const noexcept {
    return is_equal(rhs);
  }
  constexpr bool operator==(const unsigned long long rhs) const noexcept {
    return is_equal(rhs);
  }

  constexpr bool operator!=(const unsigned char rhs) const noexcept {
    return !is_equal(rhs);
  }
  constexpr bool operator!=(const unsigned short rhs) const noexcept {
    return !is_equal(rhs);
  }
  constexpr bool operator!=(const unsigned int rhs) const noexcept {
    return !is_equal(rhs);
  }
  constexpr bool operator!=(const unsigned long rhs) const noexcept {
    return !is_equal(rhs);
  }
  constexpr bool operator!=(const unsigned long long rhs) const noexcept {
    return !is_equal(rhs);
  }

  constexpr uid_t& operator++() noexcept {
    for (auto& byte : values_) {
      if (++byte != 0) {
        break;
      }
    }
    return *this;
  }

  constexpr uid_t operator++(int) noexcept {
    const auto previous = *this;
    ++*this;
    return previous;
  }

  constexpr uid_t& operator--() noexcept {
    for (auto& byte : values_) {
      if (byte-- != 0) {
        break;
      }
    }
    return *this;
  }

  constexpr uid_t operator--(int) noexcept {
    const auto previous = *this;
    --*this;
    return previous;
  }

  constexpr bool operator==(const uid_t&) const noexcept = default;
  constexpr std::strong_ordering operator<=>(const uid_t& rhs) const noexcept {
    return convert<uint64_t>() <=> rhs.convert<uint64_t>();
  }

  template <typename _Ty>
    requires can_be_uid<_Ty>
  constexpr _Ty convert() const noexcept {
    uint64_t value{};
    for (std::size_t i = 0; i < values_.size(); ++i) {
      value |= static_cast<uint64_t>(values_[i]) << (i * 8);
    }
    return static_cast<_Ty>(value);
  }

  explicit constexpr operator bool() const noexcept {
    return convert<uint64_t>() != 0;
  }

  template <std::size_t _Idx>
  constexpr uint8_t get() const {
    static_assert(_Idx < 6);
    return values_[_Idx];
  }

 private:
  std::array<uint8_t, 6> values_{};

  template <typename _Ty>
    requires can_be_uid<_Ty>
  constexpr bool is_equal(const _Ty other) const noexcept {
    return convert<uint64_t>() == static_cast<uint64_t>(other);
  }
};

static_assert(sizeof(uid_t) == 6);

template <>
struct std::hash<uid_t> {
  _NODISCARD _STATIC_CALL_OPERATOR std::size_t operator()(const uid_t& _Keyval)
      _CONST_CALL_OPERATOR noexcept {
    return std::hash<uint64_t>{}(_Keyval.convert<uint64_t>());
  }
};

#ifdef FMT_VERSION
template <>
struct fmt::formatter<uid_t> : formatter<uint64_t> {
  auto format(const uid_t& value, format_context& ctx) const {
    return formatter<uint64_t>::format(value.convert<uint64_t>(), ctx);
  }
};
#endif

template <>
class std::numeric_limits<uid_t> {
 public:
  static constexpr bool is_specialized = true;
  static constexpr bool is_signed = false;
  static constexpr bool is_integer = true;
  static constexpr bool is_exact = true;
  static constexpr bool is_modulo = true;
  static constexpr int radix = 2;
  static constexpr int digits = 48;
  static constexpr int digits10 = 14;

  static constexpr uid_t min() noexcept { return uid_t::limits::min; }
  static constexpr uid_t lowest() noexcept { return min(); }
  static constexpr uid_t max() noexcept { return uid_t::limits::max; }
};
