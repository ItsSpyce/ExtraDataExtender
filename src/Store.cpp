#include "Store.h"

#include "StringReader.h"

namespace {
using namespace ExtraDataExtender;
constexpr uint32_t SCHEMA = 'SCMA';
constexpr uint32_t REFERENCES = 'REFR';

Store::UniversalID ParseUniversalID(const uint32_t refID,
                                    const uint32_t uniqueID) {
  return static_cast<uint32_t>(refID) << 32 | static_cast<uint64_t>(uniqueID);
}

Store::UniversalID ParseUniversalID(const uint32_t refID) {
  return ParseUniversalID(refID, 0);
}

std::pair<uint32_t, uint32_t> GetUniversalIDParts(const Store::UniversalID id) {
  return {static_cast<uint32_t>(id >> 32),
          static_cast<uint32_t>(id & 0xFFFFFFFFu)};
}
}  // namespace

namespace ExtraDataExtender {
void Store::LoadFromSave(const SKSE::SerializationInterface* cosave) {
  uint32_t type, version, length;
  CosaveData data;
  bool didRead = false;
  while (cosave->GetNextRecordInfo(type, version, length)) {
    didRead = true;
    if (version == 1) {
      if (type == CosaveData::IDEN) {
        size_t cosaveLen;
        if (!cosave->ReadRecordData(cosaveLen)) {
          logger::error("Failed to read save data length");
          return;
        }
        if (cosaveLen == 0) return;
        std::string buffer(cosaveLen, NULL);
        if (!cosave->ReadRecordData(buffer.data(), cosaveLen)) {
          logger::error("No read data found");
          return;
        }
        StringReader reader(buffer);
        while (!reader.AtEnd()) {
        }
      } else if (type == SCHEMA) {
        if (!cosave->ReadRecordData(data.schema)) {
          logger::error("Failed to read schema");
          return;
        }
      } else if (type == REFERENCES) {
        size_t refrCount;
        if (!cosave->ReadRecordData(refrCount)) {
          logger::error("Failed to read reference count");
          return;
        }
      }
    }
  }
  if (didRead) {
    logger::debug("Loaded save data: {}", length);
  }
}

void Store::WriteToSave(const SKSE::SerializationInterface* cosave) {
  if (!cosave->OpenRecord(CosaveData::IDEN, 1)) {
    logger::error("Failed to open save records");
    return;
  }
  std::stringstream out;
}

void Store::CommitReference(const RE::TESObjectREFR* refr) {}

void Store::PrepareReference(const RE::TESObjectREFR* refr) {}

RE::BSEventNotifyControl Store::ProcessEvent(
    const RE::TESUniqueIDChangeEvent* a_event,
    RE::BSTEventSource<RE::TESUniqueIDChangeEvent>* a_eventSource) {
  return RE::BSEventNotifyControl::kContinue;
}

Store::UniversalID Store::GetID(RE::TESObjectREFR* refr, bool init) const {
  UniversalID id = 0;
  Guard guard(lock_);
  if (init) {
    id = ParseUniversalID(refr->GetFormID());
    cosaveData_->references.try_emplace(id, Record{});
  } else {
    if (const auto it = cosaveData_->references.find(refr->GetFormID());
        it != cosaveData_->references.end()) {
      id = it->second;
    }
  }
  return id;
}

Store::UniversalID Store::GetID(RE::InventoryEntryData* invData, bool init) {}
}  // namespace ExtraDataExtender