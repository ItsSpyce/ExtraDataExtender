#pragma once

#include <type_traits>

namespace ExtraDataExtender {
typedef uint32_t EDE_TYPE;
typedef uint8_t EDE_VERSION;

namespace internal {
enum VariableKind {
  kScalar,
  kArray,
  kStruct,
};

struct VariableDefinition {
  VariableKind kind;
  void* binding;
};
}  // namespace internal

template <class ExtraDataStruct>
class BindingContext;

class ExtraDataConstructor {
 public:
  template <class ExtraDataStruct>
  BindingContext<ExtraDataStruct> create_context(const std::string& name) {
    return BindingContext<ExtraDataStruct>{name};
  }
};

template <typename T>
concept is_extradata = requires(T) {
  std::is_trivially_constructible_v<T>;
  { T::bind(ExtraDataConstructor{}) } -> void;
};

template <class ExtraDataStruct>
class BindingContext {
 public:
  BindingContext(const std::string& name) : name_(name) {}

  template <typename Member>
  void bind(const std::string& name, Member ExtraDataStruct::* memberObjectPtr) {
    if constexpr (std::is_array_v<Member>) {
      create_variable(name, internal::VariableDefinition{.kind = internal::kArray, .binding = memberObjectPtr });
    } else if constexpr (std::is_integral_v<Member>) {
      
    } else if constexpr (std::is_same_v<std::string, Member>) {
      
    }
  }

 private:
  std::string name_;
  void create_variable(const std::string& name,
            const internal::VariableDefinition& definition);
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

  template <is_extradata ExtraDataStruct>
  void RegisterType();

  template <is_extradata ExtraDataStruct>
  bool HasExtraData(RE::TESObjectREFR* refr);

  template <is_extradata ExtraDataStruct>
  void AddExtraData(RE::TESObjectREFR* refr, const ExtraDataStruct& data);

  template <is_extradata ExtraDataStruct>
  void UpdateExtraData(RE::TESObjectREFR* refr, const ExtraDataStruct& data);
};

template <typename VersionedInterface>
  requires(std::is_assignable_v<IPluginInterface, VersionedInterface>)
VersionedInterface* Query() {
  static VersionedInterface* iface = nullptr;
  static bool didError = false;
  if (!iface && !didError) {
    if (const auto handle = GetModuleHandleW(L"ExtraDataExtender.dll")) {
      if (const auto queryFn = reinterpret_cast<void* (*)()>(
              GetProcAddress(handle, "QueryPluginInterface"))) {
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