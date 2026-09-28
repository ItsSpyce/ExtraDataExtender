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

using EDE_StatusCode = std::int32_t;
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
  virtual bool WriteRecordData(void* buffer, std::uint32_t length) const = 0;
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
ExtraDataAPI bool RefrHasExtraData(const RE::TESObjectREFR* reference,
                                   const char* id) noexcept;
ExtraDataAPI bool RefrAddExtraData(const RE::TESObjectREFR* reference,
                                   ExtraData* data) noexcept;
ExtraDataAPI ExtraData* RefrGetExtraData(const RE::TESObjectREFR* reference,
                                         const char* id) noexcept;
ExtraDataAPI bool RefrRemoveExtraData(const RE::TESObjectREFR* reference,
                                      const char* id) noexcept;
ExtraDataAPI bool ItemHasExtraData(const RE::TESObjectREFR* owner,
                                   const RE::TESBoundObject* object,
                                   const RE::ExtraDataList* instance,
                                   const char* id) noexcept;
ExtraDataAPI bool ItemAddExtraData(const RE::TESObjectREFR* owner,
                                   const RE::TESBoundObject* object,
                                   const RE::ExtraDataList* instance,
                                   ExtraData* data) noexcept;
ExtraDataAPI ExtraData* ItemGetExtraData(const RE::TESObjectREFR* owner,
                                         const RE::TESBoundObject* object,
                                         const RE::ExtraDataList* instance,
                                         const char* id) noexcept;
ExtraDataAPI bool ItemRemoveExtraData(const RE::TESObjectREFR* owner,
                                      const RE::TESBoundObject* object,
                                      const RE::ExtraDataList* instance,
                                      const char* id) noexcept;
}  // extern "C"
enum InterfaceVersion {
  InterfaceVersion_None = 0,
  InterfaceVersion_V1 = 1,
};
inline constexpr std::uint32_t InterfaceVersion = InterfaceVersion_V1;
struct IPluginInterfaceV1 {
  std::uint32_t version;
  std::uint32_t size;
  decltype(&RegisterDataType) RegisterDataType;
  decltype(&Exists) Exists;
  decltype(&RefrHasExtraData) RefrHasExtraData;
  decltype(&RefrAddExtraData) RefrAddExtraData;
  decltype(&RefrGetExtraData) RefrGetExtraData;
  decltype(&RefrRemoveExtraData) RefrRemoveExtraData;
  decltype(&ItemHasExtraData) ItemHasExtraData;
  decltype(&ItemAddExtraData) ItemAddExtraData;
  decltype(&ItemGetExtraData) ItemGetExtraData;
  decltype(&ItemRemoveExtraData) ItemRemoveExtraData;
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
      !api->Exists || !api->RefrHasExtraData || !api->RefrAddExtraData ||
      !api->RefrGetExtraData || !api->RefrRemoveExtraData ||
      !api->ItemHasExtraData || !api->ItemAddExtraData ||
      !api->ItemGetExtraData || !api->ItemRemoveExtraData)
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
inline bool RefrHasExtraData(const RE::TESObjectREFR* owner,
                             const char* id) noexcept {
  const auto [api, _] = internal::Connect();
  return api ? api->RefrHasExtraData(owner, id) : false;
}
inline bool RefrAddExtraData(const RE::TESObjectREFR* owner,
                             ExtraData* data) noexcept {
  const auto [api, _] = internal::Connect();
  return api ? api->RefrAddExtraData(owner, data) : false;
}
inline ExtraData* RefrGetExtraData(const RE::TESObjectREFR* owner,
                                   const char* id) noexcept {
  const auto [api, _] = internal::Connect();
  return api ? api->RefrGetExtraData(owner, id) : nullptr;
}
inline bool RefrRemoveExtraData(const RE::TESObjectREFR* owner,
                                const char* id) noexcept {
  const auto [api, _] = internal::Connect();
  return api ? api->RefrRemoveExtraData(owner, id) : false;
}
inline bool ItemHasExtraData(const RE::TESObjectREFR* owner,
                             const RE::TESBoundObject* object,
                             const RE::ExtraDataList* instance,
                             const char* id) noexcept {
  const auto [api, _] = internal::Connect();
  return api ? api->ItemHasExtraData(owner, object, instance, id) : false;
}
inline bool ItemAddExtraData(const RE::TESObjectREFR* owner,
                             const RE::TESBoundObject* object,
                             const RE::ExtraDataList* instance,
                             ExtraData* data) noexcept {
  const auto [api, status] = internal::Connect();
  return api ? api->ItemAddExtraData(owner, object, instance, data) : false;
}
inline ExtraData* ItemGetExtraData(const RE::TESObjectREFR* owner,
                                   const RE::TESBoundObject* object,
                                   const RE::ExtraDataList* instance,
                                   const char* id) noexcept {
  const auto [api, _] = internal::Connect();
  return api ? api->ItemGetExtraData(owner, object, instance, id) : nullptr;
}
inline bool ItemRemoveExtraData(const RE::TESObjectREFR* owner,
                                const RE::TESBoundObject* object,
                                const RE::ExtraDataList* instance,
                                const char* id) noexcept {
  const auto [api, status] = internal::Connect();
  return api ? api->ItemRemoveExtraData(owner, object, instance, id) : false;
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
bool RefrHasExtraData(const RE::TESObjectREFR* reference) noexcept {
  if (!reference) return false;
  try {
    return ExtraDataExtender::RefrHasExtraData(reference,
                                               internal::Info<T>().id.c_str());
  } catch (...) {
    return false;
  }
}

template <ExtraDataType T>
_NODISCARD bool RefrAddExtraData(const RE::TESObjectREFR* reference,
                                 T* data) noexcept {
  if (!reference || !data) return false;
  return RefrAddExtraData(reference, static_cast<ExtraData*>(data));
}

template <ExtraDataType T>
T* RefrGetExtraData(const RE::TESObjectREFR* reference) noexcept {
  if (!reference) return nullptr;
  try {
    return dynamic_cast<T*>(ExtraDataExtender::RefrGetExtraData(
        reference, internal::Info<T>().id.c_str()));
  } catch (...) {
    return nullptr;
  }
}

template <ExtraDataType T>
_NODISCARD bool RefrRemoveExtraData(
    const RE::TESObjectREFR* reference) noexcept {
  try {
    return ExtraDataExtender::RefrRemoveExtraData(
        reference, internal::Info<T>().id.c_str());
  } catch (...) {
    return false;
  }
}

template <ExtraDataType T>
bool ItemHasExtraData(const RE::TESObjectREFR* owner,
                      const RE::TESBoundObject* object,
                      const T* instance) noexcept {
  try {
    return ExtraDataExtender::ItemHasExtraData(owner, object, instance,
                                               internal::Info<T>().id.c_str());
  } catch (...) {
    return false;
  }
}

template <ExtraDataType T>
bool ItemAddExtraData(const RE::TESObjectREFR* owner,
                      const RE::TESBoundObject* object,
                      const RE::ExtraDataList* instance, T* data) noexcept {
  try {
    return ItemAddExtraData(owner, object, instance,
                            static_cast<ExtraData*>(data));
  } catch (...) {
    return false;
  }
}

/// <summary>
///   A convenience method for adding to an item. It will use only the FIRST
///   ExtraDataList entry. If you want to add to a specific one, use the default
///   overload.
/// </summary>
/// <typeparam name="T"></typeparam>
/// <param name="owner"></param>
/// <param name="object"></param>
/// <param name="data"></param>
/// <returns></returns>
template <ExtraDataType T>
bool ItemAddExtraData(const RE::TESObjectREFR* owner,
                      const RE::TESBoundObject* object, T* data) noexcept {
  try {
    if (const auto* changes =
            const_cast<RE::TESObjectREFR*>(owner)->GetInventoryChanges(true);
        changes && changes->entryList) {
      for (const auto* entry : *changes->entryList) {
        if (!entry || !entry->object || !entry->extraLists) continue;
        for (const auto* extraList : *entry->extraLists) {
          if (extraList && extraList->GetCount() > 0) {
            return ItemAddExtraData<T>(owner, object, extraList, data);
          }
        }
      }
    }
    return false;
  } catch (...) {
    return false;
  }
}

template <ExtraDataType T>
T* ItemGetExtraData(const RE::TESObjectREFR* owner,
                    const RE::TESBoundObject* object,
                    const RE::ExtraDataList* instance) noexcept {
  try {
    return dynamic_cast<T*>(ExtraDataExtender::ItemGetExtraData(
        owner, object, instance, internal::Info<T>().id.c_str()));
  } catch (...) {
    return nullptr;
  }
}

/// <summary>
///   A convenience method for getting ExtraData from an item. It will only read
///   from the FIRST ExtraDataList entry. If you want to query from a specific
///   one, use the default overload.
/// </summary>
/// <typeparam name="T"></typeparam>
/// <param name="owner"></param>
/// <param name="object"></param>
/// <returns></returns>
template <ExtraDataType T>
T* ItemGetExtraData(const RE::TESObjectREFR* owner,
                    const RE::TESBoundObject* object) noexcept {
  try {
    if (const auto* changes =
            const_cast<RE::TESObjectREFR*>(owner)->GetInventoryChanges(true);
        changes && changes->entryList) {
      for (const auto* entry : *changes->entryList) {
        if (!entry || !entry->object || !entry->extraLists) continue;
        for (const auto* extraList : *entry->extraLists) {
          if (extraList && extraList->GetCount() > 0) {
            return ItemGetExtraData<T>(owner, object, extraList);
          }
        }
      }
    }
    return nullptr;
  } catch (...) {
    return nullptr;
  }
}

template <ExtraDataType T>
bool ItemRemoveExtraData(const RE::TESObjectREFR* owner,
                         const RE::TESBoundObject* object,
                         const RE::ExtraDataList* instance) noexcept {
  try {
    return ExtraDataExtender::ItemRemoveExtraData(
        owner, object, instance, internal::Info<T>().id.c_str());
  } catch (...) {
    return false;
  }
}

/// <summary>
///   A convenience method for removing ExtraData from an item. It will only
///   read from the FIRST ExtraDataList entry. If you want to remove from a
///   specific one, use the default overload.
/// </summary>
/// <typeparam name="T"></typeparam>
/// <param name="owner"></param>
/// <param name="object"></param>
/// <returns></returns>
template <ExtraDataType T>
bool ItemRemoveExtraData(const RE::TESObjectREFR* owner,
                         const RE::TESBoundObject* object) noexcept {
  try {
    if (const auto* changes =
            const_cast<RE::TESObjectREFR*>(owner)->GetInventoryChanges(true);
        changes && changes->entryList) {
      for (const auto* entry : *changes->entryList) {
        if (!entry || !entry->object || !entry->extraLists) continue;
        for (const auto* extraList : *entry->extraLists) {
          if (extraList && extraList->GetCount() > 0) {
            return ItemRemoveExtraData<T>(owner, object, extraList);
          }
        }
      }
    }
    return false;
  } catch (...) {
    return false;
  }
}

template <ExtraDataType T>
_NODISCARD EDE_StatusCode RegisterDataType() noexcept {
  return Register<T>();
}
}  // namespace ExtraDataExtender
