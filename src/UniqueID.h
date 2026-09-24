#pragma once

#include "TypeRegistry.h"

namespace ExtraDataExtender {
// UIDs are special.
//  UID  | OWNER
// 000000|000000
inline constexpr uid_t UID_MAX = UINT64_MAX;
inline constexpr uid_t UID_NONE = NULL;

namespace UniqueID {
class PickupCtx;

thread_local inline PickupCtx* g_pickupCtx;

class PickupCtx {
 public:
  PickupCtx(const RE::TESObjectREFR* destination,
            const RE::TESObjectREFR* worldItem, const int32_t count = 1)
      : previous_(g_pickupCtx) {
    if (destination && worldItem && worldItem->GetBaseObject() && count > 0) {
      owner = destination->GetFormID();
      refr = worldItem->GetFormID();
      obj = worldItem->GetBaseObject()->GetFormID();
      this->count = count;
      isFullStack = count == worldItem->extraList.GetCount();
      if (const auto* native =
              worldItem->extraList.GetByType<RE::ExtraUniqueID>()) {
        sourceNativeUID = native->uniqueID;
      }
    }
    g_pickupCtx = this;
  }
  ~PickupCtx() { g_pickupCtx = previous_; }
  DONOTMOVEITMOVEIT(PickupCtx);

  RE::FormID owner = NULL, refr = NULL, obj = NULL;
  NativeUID sourceNativeUID = NULL, destNativeUID = NULL;
  int32_t count = NULL;
  bool isFullStack = false;

 private:
  PickupCtx* previous_;
};

class Registry;
inline Registry* g_registry{};

class Registry {
  static constexpr uint32_t get_id(const uid_t uid) {
    return static_cast<uint32_t>(uid >> 32);
  }
  static constexpr RE::FormID get_owner(const uid_t uid) {
    return static_cast<RE::FormID>(uid);
  }
  static constexpr uid_t get_uid(const uint32_t id, const RE::FormID owner) {
    return static_cast<uint64_t>(id) << 32 | static_cast<uint32_t>(owner);
  }

  using NativeKey = std::pair<uid_t, NativeUID>;

  class EventLoop final : public RE::BSTEventSink<RE::TESUniqueIDChangeEvent>,
                          public RE::BSTEventSink<RE::TESContainerChangedEvent>,
                          public RE::BSTEventSink<RE::TESFormDeleteEvent>,
                          public Singleton<EventLoop> {
    struct LoadedEvent {
      RE::ObjectRefHandle handle;
      RE::FormID formID;
    };

    struct UnloadedEvent {
      RE::FormID formID;
    };

    struct RefreshEvent {
      RE::FormID formID;
    };

    struct PickupOrDroppedEvent {
      RE::FormID previousOwner = NULL, newOwner = NULL, item = NULL,
                 refr = NULL;
      NativeUID nativeUID = NULL;
      int32_t count = 1;
    };

    using Event = std::variant<LoadedEvent, UnloadedEvent, RefreshEvent,
                               PickupOrDroppedEvent, RE::TESUniqueIDChangeEvent,
                               RE::TESFormDeleteEvent>;

   public:
    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESContainerChangedEvent* event,
        RE::BSTEventSource<RE::TESContainerChangedEvent>* eventSource)
        override {
      if (event->itemCount > 0 &&
          !event->oldContainer != !event->newContainer) {
        RE::FormID worldForm = NULL;
        auto nativeUID = event->uniqueID;
        if (const auto world = event->reference.get()) {
          worldForm = world->GetFormID();
          if (nativeUID == NULL) {
            if (const auto* worldUID =
                    world->extraList.GetByType<RE::ExtraUniqueID>()) {
              nativeUID = worldUID->uniqueID;
            }
          }
        }
        if (g_pickupCtx && event->newContainer == g_pickupCtx->owner &&
            event->baseObj == g_pickupCtx->obj) {
          worldForm = g_pickupCtx->isFullStack ? g_pickupCtx->refr : NULL;
          if (nativeUID == NULL) {
            nativeUID = g_pickupCtx->destNativeUID;
          }
        }
        if (worldForm != NULL && nativeUID != NULL) {
          Queue(PickupOrDroppedEvent{.previousOwner = event->oldContainer,
                                     .newOwner = event->newContainer,
                                     .item = event->baseObj,
                                     .refr = worldForm,
                                     .nativeUID = nativeUID,
                                     .count = event->itemCount});
        }
      }
      if (event->oldContainer != NULL) Queue(RefreshEvent{event->oldContainer});
      if (event->newContainer != NULL) Queue(RefreshEvent{event->newContainer});
      return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESFormDeleteEvent* event,
        RE::BSTEventSource<RE::TESFormDeleteEvent>* eventSource) override {
      Queue(*event);
      return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESUniqueIDChangeEvent* event,
        RE::BSTEventSource<RE::TESUniqueIDChangeEvent>* eventSource) override {
      if (g_pickupCtx && g_pickupCtx->isFullStack &&
          g_pickupCtx->owner == event->newBaseID &&
          g_pickupCtx->obj == event->objectID &&
          g_pickupCtx->sourceNativeUID == event->oldUniqueID &&
          event->newUniqueID) {
        Queue(PickupOrDroppedEvent{.previousOwner = NULL,
                                   .newOwner = g_pickupCtx->owner,
                                   .item = g_pickupCtx->obj,
                                   .refr = g_pickupCtx->refr,
                                   .nativeUID = event->newUniqueID,
                                   .count = g_pickupCtx->count});
      }
      Queue(*event);
      return RE::BSEventNotifyControl::kContinue;
    }

    void Register() {
      auto* events = RE::ScriptEventSourceHolder::GetSingleton();
      events->AddEventSink<RE::TESUniqueIDChangeEvent>(this);
      events->AddEventSink<RE::TESContainerChangedEvent>(this);
      events->AddEventSink<RE::TESFormDeleteEvent>(this);
    }

    void Reset() {
      std::scoped_lock lock{mutex_};
      ++gen_;
      queue_.clear();
      isScheduled_ = false;
      didFail_ = false;
      isAccepting_ = false;
    }

    void Start() {}

    void Queue(Event event) {}

    result<bool> Flush(uint64_t generation, bool init) _SAFE {
      auto save = Save::GetSingleton();
      try {
        std::set<RE::FormID> refresh;
        for (size_t count = 0; count < UINT16_MAX;) {
          std::deque<Event> batch;
          {
            std::scoped_lock lock{mutex_};
            if (generation != gen_) return Ok{false};
            if (didFail_) {
              // TODO: clear load
              return Err{"Cannot flush a failed loop"};
            }
            if (queue_.empty() && refresh.empty()) {
              isScheduled_ = false;
              break;
            }
            batch.swap(queue_);
          }
          if (count == 0 && !batch.empty()) {
            logger::info("Draining {} events", batch.size());
          }
          if (batch.size() > UINT16_MAX - count) {
            return Err{"Reached upper limit for drain"};
          }
          count += batch.size();
          for (const auto& event : batch) {
            if (const auto* transfer =
                    std::get_if<PickupOrDroppedEvent>(&event)) {
            }

            std::visit(
                [](const auto& value) {
                  //
                },
                event);
          }
        }
      } catch (const std::exception& err) {
        return Err{"Failed to flush unique ID queue: {}", err.what()};
      }
    }

   private:
    std::mutex mutex_;
    std::deque<Event> queue_;
    uint64_t gen_{};
    bool isAccepting_{}, isScheduled_{}, didFail_{};

    result<bool> HandleEvent(const PickupOrDroppedEvent& event,
                             std::set<RE::FormID>& refresh) {
      auto* owner = RE::TESForm::LookupByID<RE::TESObjectREFR>(
          event.previousOwner != NULL ? event.previousOwner : event.newOwner);
      if (!owner || owner->IsDeleted())
        return Err{"Owner not loaded or deleted"};
      const auto ownerID = g_registry->GetUniqueID(owner, false);
      if (!ownerID) return ownerID.error();
      const NativeKey key{ownerID.value(), event.nativeUID};
      if (event.previousOwner != NULL) {
        // drop event
        if (g_registry->FindItem(key, event.item).value_or(UID_NONE) == UID_NONE)
          return Ok{false};
        if (FindStack(owner, event.item, event.nativeUID)) return Ok{true};
        if (const auto moved = g_registry->DropItem(key, event.refr, event.item); !moved) {
          return moved.error();
        }
        if (auto* world = RE::TESForm::LookupByID<RE::TESObjectREFR>(event.refr); world && !world->IsDeleted() && world->Is3DLoaded()) {
          if (const auto loadResult = g_registry->LoadRefr(event.refr); !loadResult) {
            return loadResult.error();
          }
        }
      } else {
        // pickup
        if (g_registry->FindRefr(event.refr, event.item).value_or(UID_NONE) == UID_NONE) {
          return Ok{false};
        }
      }
      refresh.insert(owner->GetFormID());
    }

    result<bool> HandleEvent(const LoadedEvent& event,
                             std::set<RE::FormID>& refresh) {
      const auto ref = event.handle.get();
      if (ref && ref->GetFormID() == event.formID) {
      }
    }

    static RE::ExtraDataList* FindStack(RE::TESObjectREFR* owner,
                                        RE::FormID obj, NativeUID nativeUID) {
      auto* changes = owner->GetInventoryChanges(true);
      if (!changes || !changes->entryList) {
        return nullptr;
      }
      // I don't want to iterate through the whole list but there's a chance
      // duplicate UIDs could be found. In that scenario, we don't want to use
      // it.
      RE::ExtraDataList* found = nullptr;
      for (auto* entry : *changes->entryList) {
        if (!entry || !entry->object || !entry->object->GetFormID() != obj ||
            !entry->extraLists)
          continue;
        for (auto* stack : *entry->extraLists) {
          if (!stack) continue;
          const auto* id = stack->GetByType<RE::ExtraUniqueID>();
          if (id && id->baseID == owner->GetFormID() &&
              id->uniqueID == nativeUID) {
            if (found) {
              logger::warn("Found duplicate UID within stack for owner {}", owner->GetFormID());
              return nullptr;
            }
            found = stack;
          }
        }
      }
      return found;
    }
  };

 public:
  result<uid_t> GetUniqueID(const RE::TESObjectREFR* target,
                            bool init = false) _SAFE {
    if (!target) {
      return Err{"Target is null"};
    }
  }
  result<uid_t> GetUniqueID(const RE::TESObjectREFR* refr,
                            RE::ExtraDataList* itemExtraData,
                            bool init = false) _SAFE {
    if (!refr) {
      return Err{"Target is null"};
    }
    if (!itemExtraData) {
      return Err{"No item extra data"};
    }
  }
  result<uid_t> GetUniqueID(const RE::TESObjectREFR* refr, RE::InventoryEntryData* item, bool init = false) _SAFE {
    if (!refr) {
      return Err{"Target is null"};
    }
    if (!item) {
      return Err{"No inventory item"};
    }
    for (auto* stack : *item->extraLists) {
      if (!stack) continue;
      const auto* id = stack->GetByType<RE::ExtraUniqueID>();
      if (id && id->baseID == refr->GetFormID())
    }
  }
  result<uid_t> FindRefr(RE::FormID form, RE::FormID obj) const _SAFE {}
  result<uid_t> FindItem(const NativeKey& key, RE::FormID obj = 0) const _SAFE {
  }
  result<uid_t> DropItem(const RE::TESObjectREFR* refr,
                         RE::InventoryEntryData* item, bool init = false) _SAFE;
  result<uid_t> DropItem(const NativeKey& from, RE::FormID worldRefr,
                         RE::FormID obj) _SAFE;
  result<uid_t> PickupItem(const RE::TESObjectREFR* refr,
                           RE::InventoryEntryData* item,
                           bool init = false) _SAFE;
  result<uid_t> PickupItem(const NativeKey& from, const NativeKey& to,
                           RE::FormID obj) _SAFE;
  result<uid_t> MoveItem(const RE::TESObjectREFR* owner,
                         const RE::TESObjectREFR* target,
                         bool init = false) _SAFE;
  result<uid_t> MoveItem(const NativeKey& from, const NativeKey& to,
                         RE::FormID obj) _SAFE;
  result<bool> ClearUniqueID(const RE::TESObjectREFR* target) _SAFE;
  result<bool> ClearUniqueID(const RE::TESObjectREFR* owner,
                             RE::ExtraDataList* itemExtraData) _SAFE;
  result<bool> CleanupInventory(const RE::TESObjectREFR* owner,
                                const std::set<NativeUID>& present) _SAFE;
  result<bool> IsLoaded(uid_t uid) const _SAFE;
  result<bool> Load(uid_t uid) _SAFE;
  result<bool> LoadRefr(RE::FormID refrID) _SAFE;
  result<bool> Unload(uid_t uid) _SAFE;
  result<std::set<uid_t>> GetLoadedIDs() const _SAFE;
  result<bool> ClearLoadedIDs() _SAFE;
  result<bool> ClearAll() _SAFE;

 private:
  uid_t next_{1};
  std::unordered_map<RE::FormID, uid_t> references_;
  std::unordered_map<NativeKey, uid_t> items_;
  std::unordered_map<RE::FormID, std::unordered_set<uid_t>> loaded_;
  std::unordered_set<uid_t> owners_;
  TypeRegistry types_;
  bool dirty_{};

  static RE::ExtraDataList* ExtraDataList_Ctor(void* _this) {
    using func_t = decltype(ExtraDataList_Ctor);
    static const REL::Relocation<func_t> func{RELOCATION_ID(11437, 11583)};
    return func(_this);
  }

  static result<RE::ExtraDataList*> GetOrCreateExtraDataList(RE::InventoryEntryData* data) {
    if (!data) return Err{"No entry data"};
    if (!data->extraLists) {
      data->extraLists = new RE::BSSimpleList<RE::ExtraDataList*>();
    }
    if (!data->extraLists->empty()) {
      return Ok{data->extraLists->front()};
    }
    auto* alloc = RE::MemoryManager::GetSingleton()->Allocate(sizeof(RE::ExtraDataList), 0, false);
    auto* result = ExtraDataList_Ctor(alloc);
    data->AddExtraList(result);
    return Ok{result};
  }
};
}  // namespace UniqueID
}  // namespace ExtraDataExtender