#pragma once

#ifndef ExtraDataAPI
#define ExtraDataAPI
#endif

#include <RE/E/ExtraDataList.h>
#include <RE/T/TESBoundObject.h>
#include <RE/T/TESObjectREFR.h>

#include <string>
#include <type_traits>

namespace ExtraDataExtender {
inline constexpr auto PLUGIN_NAME = L"ExtraDataExtender.dll";

using EDE_StatusCode = std::uint32_t;
inline constexpr EDE_StatusCode EDE_Ok = 0;
inline constexpr EDE_StatusCode EDE_NotInstalled = 1;
inline constexpr EDE_StatusCode EDE_DuplicateKind = 2;
inline constexpr EDE_StatusCode EDE_KindNotFound = 3;
inline constexpr EDE_StatusCode EDE_InvalidArgument = 4;
inline constexpr EDE_StatusCode EDE_Error = 5;
inline constexpr EDE_StatusCode EDE_IncompatibleVersion = 6;

#define EDE_SUCCESS(rc) ((rc) == ::ExtraDataExtender::EDE_Ok)

class SerializationStream {
 public:
  virtual bool WriteRecordData(const void* buffer,
                               std::uint32_t length) const = 0;
  virtual std::uint32_t ReadRecordData(void* buffer,
                                       std::uint32_t length) const = 0;

  template <class T>
    requires(std::is_trivially_copyable_v<T> && !std::is_pointer_v<T> &&
             !std::is_member_pointer_v<T>)
  bool WriteRecordData(const T& value) const {
    static_assert(sizeof(T) <= (std::numeric_limits<std::uint32_t>::max)());
    return WriteRecordData(std::addressof(value),
                           static_cast<std::uint32_t>(sizeof(T)));
  }

  template <class T>
    requires(std::is_trivially_copyable_v<T> && !std::is_pointer_v<T> &&
             !std::is_member_pointer_v<T> && !std::is_const_v<T>)
  std::uint32_t ReadRecordData(T& value) const {
    static_assert(sizeof(T) <= UINT32_MAX);
    return ReadRecordData(std::addressof(value),
                          static_cast<std::uint32_t>(sizeof(T)));
  }

 protected:
  virtual ~SerializationStream() = default;
};

class ExtraData {
 public:
  virtual ~ExtraData() = default;
  virtual unsigned GetVersion() = 0;
  virtual const char* GetID() = 0;
  virtual bool Read(SerializationStream* stream, unsigned savedVersion) = 0;
  virtual bool Write(SerializationStream* stream) = 0;

 protected:
  ExtraData() = default;
};

namespace abi {
using Create = ExtraData* (*)() noexcept;
using Destroy = void (*)(ExtraData*) noexcept;

extern "C" {
ExtraDataAPI EDE_StatusCode RegisterDataType(const char* id, unsigned version,
                                             Create create,
                                             Destroy destroy) noexcept;
ExtraDataAPI bool Exists(const char* id, unsigned version) noexcept;
ExtraDataAPI bool HasExtraData(const RE::TESObjectREFR* reference,
                               const RE::ExtraDataList* inventory,
                               const char* id) noexcept;
ExtraDataAPI bool AddExtraData(const RE::TESObjectREFR* reference,
                               const RE::ExtraDataList* inventory,
                               ExtraData* data) noexcept;
ExtraDataAPI ExtraData* GetExtraData(const RE::TESObjectREFR* reference,
                                     const RE::ExtraDataList* inventory,
                                     const char* id) noexcept;
ExtraDataAPI bool RemoveExtraData(const RE::TESObjectREFR* reference,
                                  const RE::ExtraDataList* inventory,
                                  const char* id) noexcept;
ExtraDataAPI bool HasExtraDataForItem(const RE::TESObjectREFR* owner,
                                      const RE::TESBoundObject* object,
                                      const RE::ExtraDataList* instance,
                                      const char* id) noexcept;
ExtraDataAPI bool AddExtraDataForItem(const RE::TESObjectREFR* owner,
                                      const RE::TESBoundObject* object,
                                      const RE::ExtraDataList* instance,
                                      ExtraData* data) noexcept;
ExtraDataAPI ExtraData* GetExtraDataForItem(const RE::TESObjectREFR* owner,
                                            const RE::TESBoundObject* object,
                                            const RE::ExtraDataList* instance,
                                            const char* id) noexcept;
ExtraDataAPI bool RemoveExtraDataForItem(const RE::TESObjectREFR* owner,
                                         const RE::TESBoundObject* object,
                                         const RE::ExtraDataList* instance,
                                         const char* id) noexcept;
}  // extern "C"
inline constexpr std::uint32_t InterfaceVersion = 1;
struct IPluginInterfaceV1 {
  std::uint32_t version;
  std::uint32_t size;
  decltype(&RegisterDataType) RegisterDataType;
  decltype(&Exists) Exists;
  decltype(&HasExtraData) HasExtraData;
  decltype(&AddExtraData) AddExtraData;
  decltype(&GetExtraData) GetExtraData;
  decltype(&RemoveExtraData) RemoveExtraData;
  decltype(&HasExtraDataForItem) HasExtraDataForItem;
  decltype(&AddExtraDataForItem) AddExtraDataForItem;
  decltype(&GetExtraDataForItem) GetExtraDataForItem;
  decltype(&RemoveExtraDataForItem) RemoveExtraDataForItem;
};
extern "C" ExtraDataAPI const IPluginInterfaceV1* EDE_GetInterface(
    std::uint32_t version) noexcept;
}  // namespace abi

namespace internal {
struct Connection {
  const abi::IPluginInterfaceV1* api;
  EDE_StatusCode status;
};
inline Connection Connect() noexcept {
#ifdef EDE_BUILD_HOST
  const auto* api = abi::EDE_GetInterface(abi::InterfaceVersion);
#else
  // SKSE loads the plugin. Do not load it ourselves or cache a failed lookup.
  const auto module = GetModuleHandleW(PLUGIN_NAME);
  if (!module) return {nullptr, EDE_NotInstalled};
  const auto query = reinterpret_cast<decltype(&abi::EDE_GetInterface)>(
      GetProcAddress(module, "EDE_GetInterface"));
  if (!query) return {nullptr, EDE_IncompatibleVersion};
  const auto* api = query(abi::InterfaceVersion);
#endif
  if (!api || api->version != abi::InterfaceVersion ||
      api->size < sizeof(abi::IPluginInterfaceV1) || !api->RegisterDataType ||
      !api->Exists || !api->HasExtraData || !api->AddExtraData ||
      !api->GetExtraData || !api->RemoveExtraData ||
      !api->HasExtraDataForItem || !api->AddExtraDataForItem ||
      !api->GetExtraDataForItem || !api->RemoveExtraDataForItem)
    return {nullptr, EDE_IncompatibleVersion};
  return {api, EDE_Ok};
}
}  // namespace internal

inline EDE_StatusCode GetStatus() noexcept {
  return internal::Connect().status;
}
inline bool IsInstalled() noexcept { return EDE_SUCCESS(GetStatus()); }
inline EDE_StatusCode RegisterDataType(const char* id, const unsigned version,
                                       const abi::Create create,
                                       const abi::Destroy destroy) noexcept {
  const auto [api, status] = internal::Connect();
  return api ? api->RegisterDataType(id, version, create, destroy) : status;
}
inline bool Exists(const char* id, const unsigned version) noexcept {
  const auto [api, _] = internal::Connect();
  return api && id && *id && api->Exists(id, version);
}
inline bool HasExtraData(const RE::TESObjectREFR* owner,
                         const RE::ExtraDataList* inventory,
                         const char* id) noexcept {
  const auto [api, _] = internal::Connect();
  return api ? api->HasExtraData(owner, inventory, id) : false;
}
inline bool AddExtraData(const RE::TESObjectREFR* owner,
                         const RE::ExtraDataList* inventory,
                         ExtraData* data) noexcept {
  const auto [api, _] = internal::Connect();
  return api ? api->AddExtraData(owner, inventory, data) : false;
}
inline ExtraData* GetExtraData(const RE::TESObjectREFR* owner,
                               const RE::ExtraDataList* inventory,
                               const char* id) noexcept {
  const auto [api, _] = internal::Connect();
  return api ? api->GetExtraData(owner, inventory, id) : nullptr;
}
inline bool RemoveExtraData(const RE::TESObjectREFR* owner,
                            const RE::ExtraDataList* inventory,
                            const char* id) noexcept {
  const auto [api, _] = internal::Connect();
  return api ? api->RemoveExtraData(owner, inventory, id) : false;
}
inline bool HasExtraDataForItem(const RE::TESObjectREFR* owner,
                                const RE::TESBoundObject* object,
                                const RE::ExtraDataList* instance,
                                const char* id) noexcept {
  const auto [api, _] = internal::Connect();
  return api ? api->HasExtraDataForItem(owner, object, instance, id) : false;
}
inline bool AddExtraDataForItem(const RE::TESObjectREFR* owner,
                                const RE::TESBoundObject* object,
                                const RE::ExtraDataList* instance,
                                ExtraData* data) noexcept {
  const auto [api, status] = internal::Connect();
  return api ? api->AddExtraDataForItem(owner, object, instance, data) : false;
}
inline ExtraData* GetExtraDataForItem(const RE::TESObjectREFR* owner,
                                      const RE::TESBoundObject* object,
                                      const RE::ExtraDataList* instance,
                                      const char* id) noexcept {
  const auto [api, _] = internal::Connect();
  return api ? api->GetExtraDataForItem(owner, object, instance, id) : nullptr;
}
inline bool RemoveExtraDataForItem(const RE::TESObjectREFR* owner,
                                   const RE::TESBoundObject* object,
                                   const RE::ExtraDataList* instance,
                                   const char* id) noexcept {
  const auto [api, status] = internal::Connect();
  return api ? api->RemoveExtraDataForItem(owner, object, instance, id) : false;
}

template <class T>
concept ExtraDataType =
    std::derived_from<T, ExtraData> && std::default_initializable<T>;

namespace internal {
struct TypeInfo {
  std::string id;
  unsigned version;
};
template <ExtraDataType T>
const TypeInfo& Info() {
  static const TypeInfo info = [] {
    T instance;
    const char* id = instance.GetID();
    if (!id || !*id)
      throw std::invalid_argument("ExtraData ID must not be empty");
    return TypeInfo{std::string(id), instance.GetVersion()};
  }();
  return info;
}
}  // namespace internal

template <ExtraDataType T>
_NODISCARD EDE_StatusCode Register() noexcept {
  try {
    const auto& type = internal::Info<T>();
    return ExtraDataExtender::RegisterDataType(
        type.id.c_str(), type.version,
        []() noexcept -> ExtraData* {
          try {
            return new T{};
          } catch (...) {
            return nullptr;
          }
        },
        [](ExtraData* data) noexcept { delete static_cast<T*>(data); });
  } catch (const std::invalid_argument&) {
    return EDE_InvalidArgument;
  } catch (...) {
    return EDE_Error;
  }
}

template <ExtraDataType T>
bool Exists(const unsigned version) noexcept {
  try {
    return ExtraDataExtender::Exists(internal::Info<T>().id.c_str(), version);
  } catch (...) {
    return false;
  }
}
template <ExtraDataType T>
bool Exists() noexcept {
  try {
    const auto& type = internal::Info<T>();
    return ExtraDataExtender::Exists(type.id.c_str(), type.version);
  } catch (...) {
    return false;
  }
}

template <ExtraDataType T>
bool HasExtraData(const RE::TESObjectREFR* reference,
                  const RE::ExtraDataList* inventory = nullptr) noexcept {
  if (!reference) return false;
  try {
    return ExtraDataExtender::HasExtraData(reference, inventory,
                                           internal::Info<T>().id.c_str());
  } catch (...) {
    return false;
  }
}

template <ExtraDataType T>
_NODISCARD bool AddExtraData(
    const RE::TESObjectREFR* reference, T* data,
    const RE::ExtraDataList* inventory = nullptr) noexcept {
  if (!reference || !data) return false;
  return AddExtraData(reference, inventory, static_cast<ExtraData*>(data));
}
template <ExtraDataType T>
_NODISCARD bool AddExtraData(const RE::TESObjectREFR* reference,
                             const RE::ExtraDataList* inventory,
                             T* data) noexcept {
  return AddExtraData(reference, data, inventory);
}
template <ExtraDataType T>
_NODISCARD bool AddExtraData(
    const RE::TESObjectREFR* reference,
    const RE::ExtraDataList* inventory = nullptr) noexcept {
  if (!reference) return false;
  try {
    auto data = std::make_unique<T>();
    if (!AddExtraData(reference, data.get(), inventory)) return false;
    data.release();
    return true;
  } catch (...) {
    return false;
  }
}

template <ExtraDataType T>
T* GetExtraData(const RE::TESObjectREFR* reference,
                const RE::ExtraDataList* inventory = nullptr) noexcept {
  if (!reference) return nullptr;
  try {
    return dynamic_cast<T*>(ExtraDataExtender::GetExtraData(
        reference, inventory, internal::Info<T>().id.c_str()));
  } catch (...) {
    return nullptr;
  }
}

template <ExtraDataType T>
_NODISCARD bool RemoveExtraData(
    const RE::TESObjectREFR* reference,
    const RE::ExtraDataList* inventory = nullptr) noexcept {
  if (!reference) return false;
  try {
    return ExtraDataExtender::RemoveExtraData(reference, inventory,
                                              internal::Info<T>().id.c_str());
  } catch (...) {
    return false;
  }
}
template <ExtraDataType T>
_NODISCARD EDE_StatusCode RegisterDataType() noexcept {
  return Register<T>();
}
}  // namespace ExtraDataExtender
