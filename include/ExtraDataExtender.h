#pragma once

#ifdef LIBRARY_EXPORTS
  #define EDE_API extern "C" __declspec(dllexport)
#else
  #define EDE_API extern "C" __declspec(dllimport)
#endif

namespace ExtraDataExtender {
constexpr std::string_view PLUGIN_NAME = "ExtraDataExtender";

// this is a copy of BSExtraData for familiarity
class EDEExtraData {
public:
  EDEExtraData() = default;
  virtual ~EDEExtraData() = default;

  [[nodiscard]] virtual uint32_t GetType() const = 0;
  virtual bool IsNotEqual(const EDEExtraData* rh) const {
    return false;
  }

  bool operator==(const EDEExtraData& rh) const {
    return !IsNotEqual(&rh);
  }

  bool operator!=(const EDEExtraData& rh) const {
    return IsNotEqual(&rh);
  }
};

EDE_API EDEExtraData* Add(RE::ExtraDataList* _this, EDEExtraData* toAdd);
EDE_API bool HasType(RE::ExtraDataList* _this, uint32_t type);
EDE_API EDEExtraData* GetByType(RE::ExtraDataList* _this, uint32_t type);
EDE_API bool Remove(RE::ExtraDataList* _this, EDEExtraData* toRemove);
}  // namespace ExtraDataExtender