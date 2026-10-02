#pragma once

#ifndef ExtraDataAPI
#ifdef EDE_BUILD_HOST
#define ExtraDataAPI __declspec(dllexport)
#else
#define ExtraDataAPI
#endif
#endif

#include <RE/E/ExtraDataList.h>
#include <RE/T/TESBoundObject.h>
#include <RE/T/TESObjectREFR.h>

#include <string>
#include <type_traits>

/// <summary>
///   Shared header file for interaction with ExtraDataExtender
/// </summary>
namespace ExtraDataExtender {
/// <summary>
///   The mod name. This is stable.
/// </summary>
inline constexpr auto PLUGIN_NAME = L"ExtraDataExtender.dll";

/// <summary>
///   A status code type returned by the ABI layer. Translates directly to
///   result status codes from ABI calls.
/// </summary>
using EDE_StatusCode = std::int32_t;
inline constexpr EDE_StatusCode EDE_Ok = 0;
inline constexpr EDE_StatusCode EDE_NotInstalled = 1;
inline constexpr EDE_StatusCode EDE_DuplicateKind = 2;
inline constexpr EDE_StatusCode EDE_KindNotFound = 3;
inline constexpr EDE_StatusCode EDE_InvalidArgument = 4;
inline constexpr EDE_StatusCode EDE_Error = 5;
inline constexpr EDE_StatusCode EDE_IncompatibleVersion = 6;

#define EDE_SUCCESS(rc) ((rc) == ::ExtraDataExtender::EDE_Ok)

/// <summary>
///   Binary stream for (de)serialization of ExtraData
/// </summary>
class SerializationStream {
 public:
  /// <summary>
  ///   Serializes data to the buffer
  /// </summary>
  /// <param name="buffer">The raw byte array to commit to the stream</param>
  /// <param name="length">The length of the content to write</param>
  /// <returns>Success or not</returns>
  virtual bool WriteRecordData(const void* buffer,
                               std::uint32_t length) const = 0;
  /// <summary>
  ///   Deserializes part of the buffer into a value
  /// </summary>
  /// <param name="buffer">The target to read to</param>
  /// <param name="length">The length of the content to read</param>
  /// <returns>Success or not</returns>
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
    static_assert(sizeof(T) <= (std::numeric_limits<std::uint32_t>::max)());
    return ReadRecordData(std::addressof(value),
                          static_cast<std::uint32_t>(sizeof(T)));
  }

 protected:
  virtual ~SerializationStream() = default;
};

class ExtraData {
 public:
  virtual ~ExtraData() = default;
  /// <summary>
  ///   The version of the ExtraData. This is to be unique per ID. It is
  ///   recommended to increment when struct/class layout is changed. Runtime
  ///   changes are checked against and not supported.
  /// </summary>
  /// <returns></returns>
  virtual unsigned Version() const = 0;
  /// <summary>
  ///   The ID of the ExtraData. This it to be unique across all mods. This is
  ///   to remain static and is what is checked against when parsing persisted
  ///   data. Runtime changes are checked against and not supported.
  /// </summary>
  /// <returns></returns>
  virtual const char* ID() const = 0;
  /// <summary>
  ///   Call to deserialize the ExtraData from a binary stream.
  /// </summary>
  /// <remarks>
  ///   Streams must be fully read to the end. If any data is left over, EDE
  ///   will return an error after Read is called. Read what you write.
  /// </remarks>
  /// <param name="stream">A pointer to the stream used for reading. Null checks
  /// are not necessary.</param> <param name="savedVersion">The version the data
  /// was saved with.</param> <returns>Success</returns>
  virtual bool Read(SerializationStream* stream, unsigned savedVersion) = 0;
  /// <summary>
  ///   Call to serialize the ExtraData to a binary stream.
  /// </summary>
  /// <param name="stream">A pointer to the stream used for writing. Null checks
  /// are not necessary.</param> <returns>Success</returns>
  virtual bool Write(SerializationStream* stream) = 0;

  /// <summary>
  ///   Compares a non-null instance of extra data to this instance
  /// </summary>
  /// <param name="rhs"></param>
  /// <returns>True if not equal, else false</returns>
  virtual bool IsNotEqual(const ExtraData& rhs) = 0;

 protected:
  ExtraData() = default;
};

namespace abi {
using Create = ExtraData* (*)() noexcept;
using Destroy = void (*)(ExtraData*) noexcept;
using IsNotEqual = bool (*)(const ExtraData&) noexcept;

extern "C" {
ExtraDataAPI EDE_StatusCode RegisterDataType(const char* id, unsigned version,
                                             Create create, Destroy destroy,
                                             IsNotEqual isNotEqual) noexcept;
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
      api->size < sizeof(abi::IPluginInterfaceV1))
    return {nullptr, EDE_IncompatibleVersion};
  return {api, EDE_Ok};
}
}  // namespace internal

/// <summary>
///   Returns a connection status to ExtraDataExtender
/// </summary>
/// <returns></returns>
inline EDE_StatusCode GetStatus() noexcept {
  return internal::Connect().status;
}
/// <summary>
///   Whether ExtraDataExtender is installed
/// </summary>
/// <returns>if GetStatus == EDE_Ok</returns>
inline bool IsInstalled() noexcept { return EDE_SUCCESS(GetStatus()); }
/// <summary>
///   Registers a data type to EDE
/// </summary>
/// <param name="id">The unique identifier of the type</param>
/// <param name="version">The version tied to the identifier</param>
/// <param name="create">The constructor. Expected to be parameter-less</param>
/// <param name="destroy">The destructor</param>
/// <param name="isNotEqual">Comparator function</param>
/// <returns></returns>
inline EDE_StatusCode RegisterDataType(
    const char* id, const unsigned version, const abi::Create create,
    const abi::Destroy destroy, const abi::IsNotEqual isNotEqual) noexcept {
  const auto [api, status] = internal::Connect();
  return api ? api->RegisterDataType(id, version, create, destroy, isNotEqual)
             : status;
}
/// <summary>
///   Checks to see if a custom data type is registered with the given params
/// </summary>
/// <param name="id">The unique identifier of the type</param>
/// <param name="version">The version tied to the identifier</param>
/// <returns>True if found, false if not or error</returns>
inline bool Exists(const char* id, const unsigned version) noexcept {
  const auto [api, _] = internal::Connect();
  return api && id && *id && api->Exists(id, version);
}

// Note: each function below has its own checks on the nullability of values.
// You are free to check yourself but validity is handled on EDE's side.

/// <summary>
///   Checks if a reference has an entry of this data type
/// </summary>
/// <param name="owner"></param>
/// <param name="id">The data type unique identifier</param>
/// <returns>True if found, found if not or error</returns>
inline bool RefrHasExtraData(const RE::TESObjectREFR* owner,
                             const char* id) noexcept {
  const auto [api, _] = internal::Connect();
  return api ? api->RefrHasExtraData(owner, id) : false;
}
/// <summary>
///   Attempts to add extra data to a reference
/// </summary>
/// <param name="owner"></param>
/// <param name="data">A pointer to the extra data instance</param>
/// <returns>True if valid and added, else false</returns>
inline bool RefrAddExtraData(const RE::TESObjectREFR* owner,
                             ExtraData* data) noexcept {
  const auto [api, _] = internal::Connect();
  return api ? api->RefrAddExtraData(owner, data) : false;
}
/// <summary>
///   Attempts to get extra data for a reference
/// </summary>
/// <param name="owner"></param>
/// <param name="id">The data type unique identifier</param>
/// <returns>A pointer to the data if found and valid, else null</returns>
inline ExtraData* RefrGetExtraData(const RE::TESObjectREFR* owner,
                                   const char* id) noexcept {
  const auto [api, _] = internal::Connect();
  return api ? api->RefrGetExtraData(owner, id) : nullptr;
}
/// <summary>
///   Attempts to remove extra data from a reference
/// </summary>
/// <param name="owner"></param>
/// <param name="id">The data type unique identifier</param>
/// <returns>True if found and removed, else false</returns>
inline bool RefrRemoveExtraData(const RE::TESObjectREFR* owner,
                                const char* id) noexcept {
  const auto [api, _] = internal::Connect();
  return api ? api->RefrRemoveExtraData(owner, id) : false;
}
/// <summary>
///   Checks if an item has extra data
/// </summary>
/// <param name="owner">The inventory item owner</param>
/// <param name="object">The inventory item object</param>
/// <param name="instance">The inventory item's
/// <c>RE::ExtraDataList*</c></param> <param name="id">The data type unique
/// identifier</param> <returns>True if found and valid, else false</returns>
inline bool ItemHasExtraData(const RE::TESObjectREFR* owner,
                             const RE::TESBoundObject* object,
                             const RE::ExtraDataList* instance,
                             const char* id) noexcept {
  const auto [api, _] = internal::Connect();
  return api ? api->ItemHasExtraData(owner, object, instance, id) : false;
}
/// <summary>
///   Attempts to add extra data to an item
/// </summary>
/// <param name="owner">The inventory item owner</param>
/// <param name="object">The inventory item object</param>
/// <param name="instance">The inventory item's
/// <c>RE::ExtraDataList*</c></param> <param name="data">A pointer to the extra
/// data</param> <returns>True if found and valid, else false</returns>
inline bool ItemAddExtraData(const RE::TESObjectREFR* owner,
                             const RE::TESBoundObject* object,
                             const RE::ExtraDataList* instance,
                             ExtraData* data) noexcept {
  const auto [api, status] = internal::Connect();
  return api ? api->ItemAddExtraData(owner, object, instance, data) : false;
}
/// <summary>
///   Attempts to get extra data for an item
/// </summary>
/// <param name="owner">The inventory item owner</param>
/// <param name="object">The inventory item object</param>
/// <param name="instance">The inventory item's
/// <c>RE::ExtraDataList*</c></param> <param name="id">The data type unique
/// identifier</param> <returns>A pointer to the data if found, else
/// null</returns>
inline ExtraData* ItemGetExtraData(const RE::TESObjectREFR* owner,
                                   const RE::TESBoundObject* object,
                                   const RE::ExtraDataList* instance,
                                   const char* id) noexcept {
  const auto [api, _] = internal::Connect();
  return api ? api->ItemGetExtraData(owner, object, instance, id) : nullptr;
}
/// <summary>
///   Attempts to remove extra data from an item
/// </summary>
/// <param name="owner">The inventory item owner</param>
/// <param name="object">The inventory item object</param>
/// <param name="instance">The inventory item's
/// <c>RE::ExtraDataList*</c></param> <param name="id">The data type unique
/// identifier</param> <returns>True if valid and removed, else false</returns>
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

/// <summary>
///   Registers a custom extra data type
/// </summary>
/// <typeparam name="T"></typeparam>
/// <returns></returns>
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

/// <summary>
///   Checks to see if a custom data type is registered with the given params
/// </summary>
/// <param name="version">The version tied to the identifier</param>
/// <typeparam name="T"></typeparam>
/// <returns>True if found, false if not or error</returns>
template <ExtraDataType T>
bool Exists(const unsigned version) noexcept {
  try {
    return ExtraDataExtender::Exists(internal::Info<T>().id.c_str(), version);
  } catch (...) {
    return false;
  }
}

/// <summary>
///   Checks to see if a custom data type is registered with the given params
/// </summary>
/// <typeparam name="T"></typeparam>
/// <returns></returns>
template <ExtraDataType T>
bool Exists() noexcept {
  try {
    const auto& type = internal::Info<T>();
    return ExtraDataExtender::Exists(type.id.c_str(), type.version);
  } catch (...) {
    return false;
  }
}

/// <summary>
///   Checks if a reference has an extra data type matching
/// </summary>
/// <typeparam name="T"></typeparam>
/// <param name="reference"></param>
/// <returns>True if valid and found, else false</returns>
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

/// <summary>
///   Attempts to add custom extra data to a reference
/// </summary>
/// <typeparam name="T"></typeparam>
/// <param name="reference"></param>
/// <param name="data"></param>
/// <returns>True if valid and added, else false</returns>
template <ExtraDataType T>
_NODISCARD bool RefrAddExtraData(const RE::TESObjectREFR* reference,
                                 T* data) noexcept {
  if (!reference || !data) return false;
  return RefrAddExtraData(reference, static_cast<ExtraData*>(data));
}

/// <summary>
///   Attempts to get extra data from a reference
/// </summary>
/// <typeparam name="T"></typeparam>
/// <param name="reference"></param>
/// <returns>A pointer to the extra data if valid and found, else null</returns>
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

/// <summary>
///   Attempts to remove extra data from a reference
/// </summary>
/// <typeparam name="T"></typeparam>
/// <param name="reference"></param>
/// <returns>True if found and removed, else false</returns>
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

/// <summary>
///   Checks if an item has extra data
/// </summary>
/// <typeparam name="T"></typeparam>
/// <param name="owner">The inventory owner</param>
/// <param name="object">The inventory item</param>
/// <param name="instance">The inventory item <c>RE::ExtraDataList*</c></param>
/// <returns>True if valid and found, else false</returns>
template <ExtraDataType T>
bool ItemHasExtraData(const RE::TESObjectREFR* owner,
                      const RE::TESBoundObject* object,
                      const RE::ExtraDataList* instance) noexcept {
  try {
    return ExtraDataExtender::ItemHasExtraData(owner, object, instance,
                                               internal::Info<T>().id.c_str());
  } catch (...) {
    return false;
  }
}

/// <summary>
///   Checks if an item has extra data. Checks only the first non-null and
///   non-empty <c>RE::ExtraDataList*</c>
/// </summary>
/// <typeparam name="T"></typeparam>
/// <param name="owner">The inventory owner</param>
/// <param name="object">The inventory item</param>
/// <returns>True if valid and found, else false</returns>
template <ExtraDataType T>
bool ItemHasExtraData(const RE::TESObjectREFR* owner,
                      const RE::TESBoundObject* object) noexcept {
  try {
    if (const auto* changes =
            const_cast<RE::TESObjectREFR*>(owner)->GetInventoryChanges(true);
        changes && changes->entryList) {
      for (const auto* entry : *changes->entryList) {
        for (const auto* extraList : *entry->extraLists) {
          if (extraList && extraList->GetCount() > 0) {
            return ItemHasExtraData<T>(owner, object, extraList);
          }
        }
      }
    }
    return false;
  } catch (...) {
    return false;
  }
}

/// <summary>
///   Attempts to add extra data to an item
/// </summary>
/// <typeparam name="T"></typeparam>
/// <param name="owner">The inventory owner</param>
/// <param name="object">The inventory object</param>
/// <param name="instance">The inventory's <c>RE::ExtraDataList*</c></param>
/// <param name="data">The data to add</param>
/// <returns>True if found and valid, else false</returns>
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
///   Attempts to add extra data to an item. Checks only the first non-null and
///   non-empty <c>RE::ExtraDataList*</c>
/// </summary>
/// <typeparam name="T"></typeparam>
/// <param name="owner">The inventory owner</param>
/// <param name="object">The inventory object</param>
/// <param name="data">The inventory's <c>RE::ExtraDataList*</c></param>
/// <returns>True if found and valid, else false</returns>
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

/// <summary>
///   Attempts to get extra data for an item
/// </summary>
/// <typeparam name="T"></typeparam>
/// <param name="owner">The inventory owner</param>
/// <param name="object">The inventory object</param>
/// <param name="instance">The inventory's <c>RE::ExtraDataList*</c></param>
/// <returns>A pointer to the extra data if found and valid, else null</returns>
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
///   Attempts to get extra data for an item. Checks only the first non-null and
///   non-empty <c>RE::ExtraDataList*</c>
/// </summary>
/// <typeparam name="T"></typeparam>
/// <param name="owner">The inventory owner</param>
/// <param name="object">The inventory object</param>
/// <returns>A pointer to the extra data if found and valid, else null</returns>
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

/// <summary>
///   Attempts to remove extra data from an item
/// </summary>
/// <typeparam name="T"></typeparam>
/// <param name="owner">The inventory owner</param>
/// <param name="object">The inventory object</param>
/// <param name="instance">The inventory's <c>RE::ExtraDataList*</c></param>
/// <returns>True if found and valid, else false</returns>
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
///   Attempts to remove extra data from an item. Checks only the first non-null
///   and non-empty <c>RE::ExtraDataList*</c>
/// </summary>
/// <typeparam name="T"></typeparam>
/// <param name="owner">The inventory owner</param>
/// <param name="object">The inventory object</param>
/// <returns>True if found and valid, else false</returns>
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

/// <summary>
///   Registers a data type
/// </summary>
/// <typeparam name="T"></typeparam>
/// <returns></returns>
template <ExtraDataType T>
_NODISCARD EDE_StatusCode RegisterDataType() noexcept {
  return Register<T>();
}
}  // namespace ExtraDataExtender
