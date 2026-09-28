#pragma once

namespace ExtraDataExtender {
class StringWriter {
 public:
  _NODISCARD const std::string& ToString() const { return bytes_; }
  _NODISCARD const std::string& Take() const & { return std::move(bytes_); }
  _NODISCARD const std::string&& Take() const && { return std::move(bytes_); }

  template <typename T>
  StringWriter& Write(T value, const size_t size = sizeof(T)) {
    if constexpr (std::is_integral_v<T>) {
      auto bits = static_cast<std::uint64_t>(value);
      for (size_t i = 0; i < size; ++i) {
        bytes_.push_back(static_cast<char>(bits & 0xFF));
        bits >>= 8;
      }
    } else if constexpr (std::is_same_v<std::decay_t<T>, std::string> ||
                         std::is_same_v<std::decay_t<T>, const char*>) {
      bytes_.append(value);
    }
    return *this;
  }

 private:
  std::string bytes_;
};
}  // namespace ExtraDataExtender
