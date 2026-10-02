#pragma once

#include "StringWriter.h"

namespace ExtraDataExtender {
class StringReader {
  friend class StringWriter;

  template <class T>
    requires std::is_integral_v<T>
  T ReadInteger(const size_t size = sizeof(T)) {
    if (size > sizeof(T)) throw std::invalid_argument("integer size overflow");
    CheckSize(size);
    std::uint64_t value{};
    for (size_t i = 0; i < size; ++i) {
      value |= static_cast<std::uint64_t>(
                   static_cast<unsigned char>(bytes_[position_ + i]))
               << (8 * i);
    }
    position_ += size;
    return static_cast<T>(value);
  }

  void CheckSize(const size_t length) const {
    if (length > Remaining()) throw std::runtime_error("truncated string");
  }

public:
  explicit StringReader(const std::string_view bytes) : bytes_(bytes) {}
  explicit StringReader(const StringWriter& writer) : bytes_(writer.ToString()) {}

  _NODISCARD size_t Remaining() const { return bytes_.size() - position_; }
  _NODISCARD bool AtEnd() const { return position_ == bytes_.size(); }
  void Skip(const size_t count) { position_ += count; }

  _NODISCARD std::string ReadString(const size_t length) {
    return std::string{ReadStringView(length)};
  }

  _NODISCARD std::string_view ReadStringView(const size_t length) {
    CheckSize(length);
    const auto value = bytes_.substr(position_, length);
    position_ += length;
    return value;
  }

  _NODISCARD std::string ReadToEnd() {
    if (AtEnd()) {
      return "";
    }
    std::string value{bytes_.substr(position_)};
    position_ = bytes_.size();
    return value;
  }

#define READER(_TYPE, _NAME) \
  _NODISCARD _TYPE _NAME() { return ReadInteger<_TYPE>(); }

  READER(uint8_t, ReadByte);
  READER(uint16_t, ReadUShort);
  READER(uint32_t, ReadUInt);
  READER(uint64_t, ReadULong);
  READER(int8_t, ReadSByte);
  READER(int16_t, ReadShort);
  READER(int32_t, ReadInt);
  READER(int64_t, ReadLong);
  _NODISCARD uint64_t ReadUID() { return ReadInteger<uint64_t>(6); }

 private:
  std::string_view bytes_;
  std::size_t position_ = 0;
};
}
