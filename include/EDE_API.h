#pragma once

#include <any>

namespace ExtraDataExtender {
typedef uint32_t EDE_TYPE;
typedef uint8_t EDE_VERSION;

class EDESaveData {
 public:
  virtual bool Write(const std::string& field, const std::any& value) = 0;
  virtual std::any Read(const std::string& field) = 0;
  virtual bool Contains(const std::string& field) = 0;

  // helpers

  template <typename Enum, typename U = std::underlying_type_t<Enum>>
  bool Write(const std::string& field, REX::EnumSet<Enum, U> enumSet) {
    return Write(field, enumSet.underlying());
  }

  // collections
  template <typename T>
  bool WriteVector(const std::string& field, const std::vector<T>& values) {
    if (!Write(field + "_len", values.size())) {
      return false;
    }
    for (size_t i = 0; i < values.size(); ++i) {
      if (!Write(field + "_" + std::to_string(i), values[i])) {
        return false;
      }
    }
    return true;
  }

  template <typename T>
  std::vector<T> ReadVector(const std::string& field) {
    std::vector<T> result;
    if (const auto length = Read(field + "_len"); length.type() == typeid(size_t)) {
      const auto size = std::any_cast<size_t>(length);
      result.resize(size);
      for (size_t i = 0; i < size; ++i) {
        result[i] = Read(field + "_" + std::to_string(i));
      }
    }
    return result;
  }
};

// this is a copy of BSExtraData for familiarity
class EDEExtraData {
 public:
  EDEExtraData() = default;
  virtual ~EDEExtraData() = default;

  [[nodiscard]] virtual EDE_TYPE GetType() const = 0;
  virtual bool IsNotEqual(const EDEExtraData* rh) const { return false; }

  bool operator==(const EDEExtraData& rh) const { return !IsNotEqual(&rh); }

  bool operator!=(const EDEExtraData& rh) const { return IsNotEqual(&rh); }

  // Add
  virtual bool Save(EDESaveData& saveData) const = 0;
  virtual bool Load(const EDESaveData& saveData) = 0;
};

class IPluginInterface {
 public:
  enum Version : EDE_VERSION {
    v1,
    latest,
  };

  virtual ~IPluginInterface() = default;

  virtual EDE_VERSION GetVersion() const = 0;
};

class EDEPluginInterfaceV1 : public IPluginInterface {
 public:
  static constexpr auto VERSION = v1;
  EDE_VERSION GetVersion() const override { return VERSION; }

  virtual EDEExtraData* Add(RE::ExtraDataList* _this, EDEExtraData* toAdd);
  virtual bool HasType(RE::ExtraDataList* _this, EDE_TYPE type);
  virtual EDEExtraData* GetByType(RE::ExtraDataList* _this, EDE_TYPE type);
  virtual bool Remove(RE::ExtraDataList* _this, EDEExtraData* toRemove);
  virtual bool RemoveByType(RE::ExtraDataList* _this, EDE_TYPE type);

  // template helpers
  template <class T>
    requires(std::is_assignable_v<EDEExtraData, T>)
  T* Add(RE::ExtraDataList* _this, T* toAdd) {
    return (T*)Add(_this, T::TYPE, toAdd);
  }

  template <class T>
    requires(std::is_assignable_v<EDEExtraData, T>)
  bool HasType(RE::ExtraDataList* _this) {
    return HasType(_this, T::TYPE);
  }

  template <class T>
    requires(std::is_assignable_v<EDEExtraData, T>)
  T* GetByType(RE::ExtraDataList* _this) {
    return GetByType(_this, T::TYPE);
  }

  template <class T>
    requires(std::is_assignable_v<EDEExtraData, T>)
  bool RemoveByType(RE::ExtraDataList* _this) {
    return RemoveByType(_this, T::TYPE);
  }
};

template <typename VersionedInterface>
  requires(std::is_assignable_v<IPluginInterface, VersionedInterface>)
VersionedInterface* Query() {
  static VersionedInterface* iface = nullptr;
  static bool didError = false;
  if (!iface && !didError) {
    if (const auto handle = GetModuleHandleW(L"ExtraDataExtender.dll")) {
      if (const auto queryFn =
              (void* (*)())GetProcAddress(handle, "QueryPluginInterface")) {
        iface = static_cast<VersionedInterface*>(queryFn());
        const auto installedVersion = iface->GetVersion();
        if (installedVersion < VersionedInterface::VERSION) {
          SKSE::log::error(
              "Installed version of EDE is lower than the minimum required");
          didError = true;
        }
      } else {
        SKSE::log::error("Failed to fetch QueryPluginInterface handle");
        didError = true;
      }
    } else {
      SKSE::log::error("Failed to locate EDE module");
      didError = true;
    }
  }
  return iface;
}
}  // namespace ExtraDataExtender