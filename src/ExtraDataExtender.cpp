#include "ExtraDataExtender.h"

#include "IDSolver.h"
#include "State.h"

namespace ExtraDataExtender::abi {
namespace {
void ReportError(const Err& err, const std::string& additionalContext) {
  switch (err.code) {
    case EDE_InvalidArgument:
      logger::error("Invalid argument error. {}. {}", additionalContext,
                    err.message);
      break;
    case EDE_DuplicateKind:
      logger::error("Duplicate kind error. {}. {}", additionalContext,
                    err.message);
      break;
    case EDE_IncompatibleVersion:
      logger::error("Incompatible version error. {}. {}", additionalContext,
                    err.message);
      break;
    case EDE_KindNotFound:
      logger::error("Unknown ExtraData kind error. {}. {}", additionalContext,
                    err.message);
      break;
    case EDE_NotInstalled:
      logger::error("EDE not installed. {}. {}", additionalContext,
                    err.message);
      break;
    case EDE_Error:
      logger::error("Uncategorized error. {}. {}", additionalContext,
                    err.message);
      break;
    default:
      break;
  }
}

uid_t Resolve(const RE::TESObjectREFR* refr) {
  if (!IDSolver::ProcessPending()) {
    logger::error("Attempted to resolve unique ID when not ready");
    return NULL;
  }
  if (const auto uid = IDSolver::ParseUniqueID(refr)) {
    return uid.value();
  } else {
    ReportError(uid.error(), "Failed to resolve unique ID for refr: {}");
    return NULL;
  }
}

uid_t Resolve(const RE::TESObjectREFR* refr, const RE::ExtraDataList* stack) {
  if (!IDSolver::ProcessPending()) {
    logger::error("Attempted to resolve unique ID when not ready");
    return NULL;
  }
  if (const auto uid = IDSolver::ParseUniqueID(refr, stack)) {
    return uid.value();
  } else {
    ReportError(uid.error(), "Failed to resolve unique ID: {}");
    return NULL;
  }
}

uid_t Resolve(const RE::TESObjectREFR* owner, const RE::TESBoundObject* obj,
              const RE::ExtraDataList* stack) {
  if (!IDSolver::ProcessPending()) {
    logger::error("Attempted to resolve unique ID when not ready");
    return NULL;
  }
  if (const auto uid = IDSolver::ParseUniqueID(owner, obj, stack)) {
    return uid.value();
  } else {
    ReportError(uid.error(), "Failed to resolve unique ID: {}");
    return NULL;
  }
}

IDStore& IDs() { return State::GetSingleton()->GetIDStore(); }
}  // namespace

EDE_StatusCode RegisterDataType(const char* id, unsigned version, Create create,
                                Destroy destroy) noexcept {
  if (const auto result = IDs().Register(id, version, create, destroy)) {
    return EDE_Ok;
  } else {
    ReportError(result.error(), "Failed to register custom ExtraData type");
    return result.error().code;
  }
}

bool Exists(const char* id, unsigned version) noexcept {
  if (const auto result = IDs().Exists(id, version)) {
    return result.value();
  } else {
    ReportError(result.error(),
                "Failed to check if ExtraData type is registered");
    return false;
  }
}

bool RefrHasExtraData(const RE::TESObjectREFR* reference,
                      const char* id) noexcept {
  if (const auto result = IDs().Has(Resolve(reference), id)) {
    return result.value();
  } else {
    ReportError(result.error(),
                "Failed to check if reference has ExtraData type");
    return false;
  }
}

bool RefrAddExtraData(const RE::TESObjectREFR* reference,
                      ExtraData* data) noexcept {
  if (const auto result = IDs().Add(Resolve(reference), data)) {
    return result.value();
  } else {
    ReportError(result.error(), "Failed to add ExtraData to reference");
    return false;
  }
}

ExtraData* RefrGetExtraData(const RE::TESObjectREFR* reference,
                            const char* id) noexcept {
  if (const auto result = IDs().Get(Resolve(reference), id)) {
    return result.value();
  } else {
    ReportError(result.error(), "Failed to get ExtraData for reference");
    return nullptr;
  }
}

bool RefrRemoveExtraData(const RE::TESObjectREFR* reference,
                         const char* id) noexcept {
  if (const auto result = IDs().Remove(Resolve(reference), id)) {
    return result.value();
  } else {
    ReportError(result.error(), "Failed to remove ExtraData from reference");
    return false;
  }
}

bool ItemHasExtraData(const RE::TESObjectREFR* owner,
                      const RE::TESBoundObject* object,
                      const RE::ExtraDataList* instance,
                      const char* id) noexcept {
  if (const auto result = IDs().Has(Resolve(owner, object, instance), id)) {
    return result.value();
  } else {
    ReportError(result.error(), "Failed to check if item has ExtraData type");
    return false;
  }
}

bool ItemAddExtraData(const RE::TESObjectREFR* owner,
                      const RE::TESBoundObject* object,
                      const RE::ExtraDataList* instance,
                      ExtraDataExtender::ExtraData* data) noexcept {
  if (const auto result = IDs().Add(Resolve(owner, object, instance), data)) {
    return result.value();
  } else {
    ReportError(result.error(), "Failed to add ExtraData to item");
    return false;
  }
}

ExtraData* ItemGetExtraData(const RE::TESObjectREFR* owner,
                            const RE::TESBoundObject* object,
                            const RE::ExtraDataList* instance,
                            const char* id) noexcept {
  if (const auto result = IDs().Get(Resolve(owner, object, instance), id)) {
    return result.value();
  } else {
    ReportError(result.error(), "Failed to get ExtraData for item");
    return nullptr;
  }
}

bool ItemRemoveExtraData(const RE::TESObjectREFR* owner,
                         const RE::TESBoundObject* object,
                         const RE::ExtraDataList* instance,
                         const char* id) noexcept {
  if (const auto result = IDs().Remove(Resolve(owner, object, instance), id)) {
    return result.value();
  } else {
    ReportError(result.error(), "Failed to remove ExtraData for item");
    return false;
  }
}

const IPluginInterfaceV1* EDE_GetInterface(uint32_t version) noexcept {
  static constexpr IPluginInterfaceV1 api{
      .version = InterfaceVersion,
      .size = sizeof(IPluginInterfaceV1),
      .RegisterDataType = &RegisterDataType,
      .Exists = &Exists,
      .RefrHasExtraData = &RefrHasExtraData,
      .RefrAddExtraData = &RefrAddExtraData,
      .RefrGetExtraData = &RefrGetExtraData,
      .RefrRemoveExtraData = &RefrRemoveExtraData,
      .ItemHasExtraData = &ItemHasExtraData,
      .ItemAddExtraData = &ItemAddExtraData,
      .ItemGetExtraData = &ItemGetExtraData,
      .ItemRemoveExtraData = &ItemRemoveExtraData,
  };
  if (version == InterfaceVersion_V1) {
    return &api;
  }
  return nullptr;
}
}  // namespace ExtraDataExtender::abi