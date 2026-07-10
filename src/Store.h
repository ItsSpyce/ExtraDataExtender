#pragma once

#include <emhash/hash_table7.hpp>
#include <emilib/emihmap2.hpp>
#include <emhash/hash_table8.hpp>
#include <lmdb-safe/lmdb-safe.hh>

#include "PluginInterfaceImpl.h"
#include "STL.h"

namespace ExtraDataExtender {
class Store : public stl::Singleton<Store>,
              public RE::BSTEventSink<RE::TESUniqueIDChangeEvent> {
 public:
  typedef uint64_t UniversalID;
  struct CosaveData {
    static constexpr uint32_t IDEN = 'EDE';
    uint16_t schema;
    std::unordered_map<RE::RefHandle, UniversalID> references;
  };

  void LoadFromSave(const SKSE::SerializationInterface* cosave);
  void WriteToSave(const SKSE::SerializationInterface* cosave);
  void PrepareReference(const RE::TESObjectREFR* refr);
  void CommitReference(const RE::TESObjectREFR* refr);

  RE::BSEventNotifyControl ProcessEvent(
      const RE::TESUniqueIDChangeEvent* a_event,
      RE::BSTEventSource<RE::TESUniqueIDChangeEvent>* a_eventSource) override;

 private:
  using Lock = std::mutex;
  using Guard = std::scoped_lock;
  // Each record is a one-to-one to a UID
  struct Record {
    emhash8::HashMap<
        std::string,
        emhash8::HashMap<std::string, PluginInterfaceImpl::ExtraDataRecord>>
        values;
  };

  Lock lock_;
  std::unique_ptr<CosaveData> cosaveData_;
  emilib2::HashMap<UniversalID, Record> loaded_;
  std::shared_ptr<MDBEnv> lmdbEnv_ = std::shared_ptr<MDBEnv>();

  UniversalID GetID(RE::TESObjectREFR* refr, bool init = false) const;
  UniversalID GetID(RE::InventoryEntryData* invData, bool init = false);
};
}  // namespace ExtraDataExtender