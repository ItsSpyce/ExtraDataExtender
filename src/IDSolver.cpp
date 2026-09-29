#include "IDSolver.h"

#include "State.h"

namespace ExtraDataExtender {
namespace {
thread_local PickupCtx* g_pickupCtx{};
struct Load {
  RE::ObjectRefHandle handle;
  RE::FormID form;
};
struct Unload {
  RE::FormID form;
};
struct Refresh {
  RE::FormID form;
};
struct WorldTransfer {
  RE::FormID oldOwner{}, newOwner{}, object{}, refr{};
  NativeUID native{};
  int32_t count{};
};
using Event = std::variant<Load, Unload, Refresh, WorldTransfer,
                           RE::TESUniqueIDChangeEvent, RE::TESFormDeleteEvent>;

class Coordinator final : public RE::BSTEventSink<RE::TESUniqueIDChangeEvent>,
                          public RE::BSTEventSink<RE::TESContainerChangedEvent>,
                          public RE::BSTEventSink<RE::TESFormDeleteEvent> {
  /**
   * In a lot of places, you'll see `gen`. This is only used to identify which
   * SKSE task queue it was called with and ensure we're still working with the
   * right generation of data
   */

 public:
  void Reset() {
    std::scoped_lock lock{mutex_};
    ++gen_;
    pending_.clear();
    scheduled_ = false;
    failure_.reset();
    accepting_ = false;
    saving_ = false;
    State::GetSingleton()->GetIDSolver().ClearLoaded();
  }
  void Start() {
    std::scoped_lock lock{mutex_};
    accepting_ = true;
  }
  void Queue(Event event) {
    size_t gen;
    {
      std::scoped_lock lock{mutex_};
      if (!accepting_ || failure_) return;
      if (pending_.size() >= UINT16_MAX) {
        failure_ = Err{"Identity queue overflow"};
        logger::error("EDE queue overflow reached");
        return;
      }
      pending_.push_back(std::move(event));
      if (saving_ || scheduled_) return;
      scheduled_ = true;
      gen = gen_;
    }
    SKSE::GetTaskInterface()->AddTask(
        [gen, this] { IGNORE(RunDrain(gen, true)); });
  }
  result<void> PrepareSave() {
    size_t gen, cutoff;
    {
      std::scoped_lock lock{mutex_};
      if (failure_) return *failure_;
      if (saving_) return Err{"Save already in progress"};
      saving_ = true;
      gen = gen_;
      cutoff = pending_.size();
    }
    return RunDrain(gen, false, cutoff);
  }
  void FinishSave() {
    size_t gen{};
    bool schedule = false;
    {
      std::scoped_lock lock{mutex_};
      if (!saving_) return;
      saving_ = false;
      if (!failure_ && !pending_.empty() && !scheduled_) {
        scheduled_ = true;
        gen = gen_;
        schedule = true;
      }
    }
    if (schedule)
      SKSE::GetTaskInterface()->AddTask(
          [gen, this] { IGNORE(RunDrain(gen, true)); });
  }
  result<void> ProcessPending() {
    size_t gen;
    {
      std::scoped_lock lock{mutex_};
      if (failure_) return *failure_;
      if (saving_) return Err{"Save already in progress"};
      if (pending_.empty() && !State::GetSingleton()->GetIDStore().NeedsSync())
        return Ok{};
      gen = gen_;
    }
    return RunDrain(gen, true);
  }
  result<void> RunDrain(size_t gen, bool createNative,
                        std::optional<size_t> cutoff = std::nullopt) {
    {
      std::scoped_lock lock{mutex_};
      if (draining_) return Err{"Drain already in progress"};
      draining_ = true;
    }
    struct DrainScope {
      Coordinator& coordinator;
      ~DrainScope() {
        std::scoped_lock lock(coordinator.mutex_);
        coordinator.draining_ = false;
      }
    } scope{*this};
    if (const auto drained = Drain(gen, createNative, cutoff);
        drained || drained.error().code == IDSolver::Err_RetryableFailure) {
      return drained;
    } else {
      return CloseWithError(drained.error());
    }
  }
  result<void> Drain(size_t gen, bool createNative,
                     std::optional<size_t> cutoff) {
    auto* state = State::GetSingleton();
    const auto& solver = state->GetIDSolver();
    std::set<RE::FormID> refresh;
    size_t processed = 0;
    for (;;) {
      std::deque<Event> batch;
      {
        std::scoped_lock lock{mutex_};
        if (gen != gen_) return Ok{};
        if (failure_) return *failure_;
        if (!state->GetDb().IsReady())
          return Err{
              "Database not ready, Drain might have been called too early"};
        if (cutoff) {
          const auto count =
              *cutoff < pending_.size() ? *cutoff : pending_.size();
          for (size_t i = 0; i < count; ++i) {
            batch.push_back(std::move(pending_.front()));
            pending_.pop_front();
          }
          *cutoff -= count;
        } else {
          batch.swap(pending_);
        }
        if (batch.empty() && refresh.empty()) {
          if (!cutoff) scheduled_ = false;
          break;
        }
      }
      if (processed == 0 && !batch.empty())
        logger::info("EDE identity drain: scanning {} queued events",
                     batch.size());
      // don't over process
      if (batch.size() > UINT16_MAX - processed)
        return Err{"Batch size limit reached"};
      processed += batch.size();
      // Correlate explicit world handles before a rekey/delete event retires
      // the old anchor, or Load allocates a new identity for a dropped ref.
      for (const auto& event : batch) {
        if (const auto* transfer = std::get_if<WorldTransfer>(&event)) {
          if (const auto transferred = BeforeApply(*transfer, refresh);
              !transferred)
            return transferred.error();
        }
      }
      for (const auto& event : batch) {
        const auto applied = std::visit(
            [&](const auto& value) { return Apply(value, refresh); }, event);
        if (!applied) return applied.error();
      }
      for (const auto form : refresh) {
        if (auto* owner = RE::TESForm::LookupByID<RE::TESObjectREFR>(form);
            owner && !owner->IsDeleted()) {
          if (const auto refreshed = RefreshInventory(*owner, createNative);
              !refreshed)
            return refreshed.error();
        }
      }
      refresh.clear();
      if (cutoff && *cutoff == 0) break;
    }
    if (const auto committed = state->CommitIDs(); !committed)
      return committed.error();
    auto& store = State::GetSingleton()->GetIDStore();
    if (processed == 0 && !store.NeedsSync()) return Ok{};
    if (const auto didSync = store.Sync(solver.LoadedIDs()); !didSync)
      return didSync.error();
    return Ok{};
  }
  result<void> CloseWithError(const Err& error) {
    IDs().ClearLoaded();
    bool alert = false;
    {
      std::scoped_lock lock{mutex_};
      if (!failure_) {
        alert = error.code == IDSolver::Err_TransferConflict;
        failure_ = error;
      }
      pending_.clear();
      scheduled_ = false;
    }
    logger::error("EDE identity processing failed: {}", error);
    if (alert)
      RE::MessageBoxMenu::Create(
          "ExtraDataExtender has detected an error during either plugin "
          "initialization or normal running. Any mods that use EDE will not "
          "work for the duration of this play session. If restarting your game "
          "does not resolve the issue, please find the log file in 'My "
          "Games/Skyrim Special Edition/SKSE/ExtraDataExtender.log' and "
          "include it in your bug report.",
          nullptr, 0, 0, 10, "OK");
    return error;
  }

  RE::BSEventNotifyControl ProcessEvent(
      const RE::TESUniqueIDChangeEvent* event,
      RE::BSTEventSource<RE::TESUniqueIDChangeEvent>*) override {
    if (g_pickupCtx && g_pickupCtx->isFull &&
        event->newBaseID == g_pickupCtx->owner &&
        event->objectID == g_pickupCtx->obj &&
        event->oldUniqueID == g_pickupCtx->srcNative && event->newUniqueID) {
      g_pickupCtx->destNative = event->newUniqueID;
      Queue(WorldTransfer{.oldOwner = 0,
                          .newOwner = g_pickupCtx->owner,
                          .object = g_pickupCtx->obj,
                          .refr = g_pickupCtx->refr,
                          .native = event->newUniqueID,
                          .count = g_pickupCtx->count});
    }
    Queue(*event);
    return RE::BSEventNotifyControl::kContinue;
  }
  RE::BSEventNotifyControl ProcessEvent(
      const RE::TESContainerChangedEvent* event,
      RE::BSTEventSource<RE::TESContainerChangedEvent>*) override {
    if (event->itemCount > 0 &&
        (!event->oldContainer != !event->newContainer)) {
      RE::FormID worldForm{};
      auto native = event->uniqueID;
      if (const auto world = event->reference.get();
          world && world->GetBaseObject() &&
          world->GetBaseObject()->GetFormID() == event->baseObj) {
        worldForm = world->GetFormID();
        if (!native)
          if (const auto* anchor =
                  world->extraList.GetByType<RE::ExtraUniqueID>())
            native = anchor->uniqueID;
      }
      if (g_pickupCtx && event->newContainer == g_pickupCtx->owner &&
          event->baseObj == g_pickupCtx->obj) {
        worldForm = g_pickupCtx->refr;
        if (!native) native = g_pickupCtx->destNative;
      }
      if (g_pickupCtx && !g_pickupCtx->isFull &&
          event->newContainer == g_pickupCtx->owner &&
          event->baseObj == g_pickupCtx->obj)
        worldForm = 0;
      if (worldForm && native)
        Queue(WorldTransfer{.oldOwner = event->oldContainer,
                            .newOwner = event->newContainer,
                            .object = event->baseObj,
                            .refr = worldForm,
                            .native = native,
                            .count = event->itemCount});
    }
    if (event->oldContainer) Queue(Refresh{event->oldContainer});
    if (event->newContainer) Queue(Refresh{event->newContainer});
    return RE::BSEventNotifyControl::kContinue;
  }
  RE::BSEventNotifyControl ProcessEvent(
      const RE::TESFormDeleteEvent* event,
      RE::BSTEventSource<RE::TESFormDeleteEvent>*) override {
    Queue(*event);
    return RE::BSEventNotifyControl::kContinue;
  }

 private:
  static IDSolver& IDs() { return State::GetSingleton()->GetIDSolver(); }
  static result<uid_t> Reference(RE::TESObjectREFR& ref) {
    if (ref.IsDeleted()) {
      return Err{"Tried to get deleted reference"};
    }
    const auto* base = ref.GetBaseObject();
    if (!base) {
      return Err{"Base object returned null"};
    }
    return IDs().Reference(ref.GetFormID(), base->GetFormID());
  }
  static result<RE::ExtraDataList*> FindStack(RE::TESObjectREFR& owner,
                                              RE::FormID object,
                                              NativeUID native) {
    const auto* changes = owner.GetInventoryChanges(true);
    if (!changes || !changes->entryList) return Ok{nullptr};
    RE::ExtraDataList* found{};

    // IMPORTANT NOTE: if someone is attempting to apply extra data to multiples
    // of an item, it is THEIR responsibility to manage that stack. EDE will
    // _not_ do it for you. "But whyyyyyyyyy?!" Because I don't want to be in
    // charge of that. There's a lot to track that could change per extra data
    // type basis and tbh I don't want to right now. Maybe later, but not now.
    for (const auto* entry : *changes->entryList) {
      if (!entry || !entry->object || entry->object->GetFormID() != object ||
          !entry->extraLists)
        continue;
      for (auto* stack : *entry->extraLists) {
        if (!stack || stack->GetCount() <= 0) continue;
        if (const auto* id = stack->GetByType<RE::ExtraUniqueID>();
            id && id->baseID == owner.GetFormID() && id->uniqueID == native) {
          if (found)
            return Err{IDSolver::Err_TransferConflict,
                       "Duplicate inventory anchor found"};
          found = stack;
        }
      }
    }
    return Ok{found};
  }
  static result<void> Apply(const WorldTransfer&, std::set<RE::FormID>&) {
    // intentionally left blank, actual work occurs in BeforeApply
    return Ok{};
  }

  static result<void> BeforeApply(const WorldTransfer& event,
                                  std::set<RE::FormID>& refresh) {
    auto* owner = RE::TESForm::LookupByID<RE::TESObjectREFR>(
        event.oldOwner ? event.oldOwner : event.newOwner);
    if (!owner || owner->IsDeleted()) return Ok{};
    const auto ownerID = Reference(*owner);
    if (!ownerID)
      return Err{"Failed to get owner reference. {}", ownerID.error()};
    const IDSolver::NativeKey key{ownerID.value(), event.native};
    if (event.oldOwner) {
      if (const auto stack = FindStack(*owner, event.object, event.native)) {
        if (stack.value()) return Ok{};
        if (const auto moved = IDs().DropItem(key, event.refr, event.object);
            !moved) {
          if (moved.error().code == IDSolver::Err_NotFound) return Ok{};
          return Err{IDSolver::Err_TransferConflict,
                     "Failed to correlate drop. {}", moved.error()};
        }
        if (const auto* world =
                RE::TESForm::LookupByID<RE::TESObjectREFR>(event.refr);
            world && !world->IsDeleted() && world->Is3DLoaded())
          IDs().LoadReference(event.refr);
      }

    } else {
      const auto stack = FindStack(*owner, event.object, event.native);
      if (!stack) return stack.error();
      if (!stack.value()) {
        return Err{IDSolver::Err_TransferConflict, "Stack returned no value"};
      }
      if (stack.value()->GetCount() != event.count) {
        return Err{IDSolver::Err_TransferConflict,
                   "Count mismatch during pickup event"};
      }
      if (const auto moved =
              IDs().PickupReference(event.refr, key, event.object);
          !moved)
        return Err{IDSolver::Err_TransferConflict,
                   "Pickup correlation failed. {}", moved.error()};
    }
    refresh.insert(owner->GetFormID());
    return Ok{};
  }

  static result<void> Apply(const Load& event, std::set<RE::FormID>& refresh) {
    if (const auto ref = event.handle.get();
        ref && ref->GetFormID() == event.form && ref->Is3DLoaded()) {
      if (const auto id = Reference(*ref); !id) return id.error();
      IDs().LoadReference(event.form);
      refresh.insert(event.form);
    }
    return Ok{};
  }

  static result<void> Apply(const Unload& event,
                            std::set<RE::FormID>& refresh) {
    IDs().UnloadReference(event.form);
    refresh.erase(event.form);
    return Ok{};
  }
  static result<void> Apply(const Refresh& event,
                            std::set<RE::FormID>& refresh) {
    refresh.insert(event.form);
    return Ok{};
  }
  static result<void> Apply(const RE::TESFormDeleteEvent& event,
                            std::set<RE::FormID>& refresh) {
    IDs().ForgetReference(event.formID);
    refresh.erase(event.formID);
    return Ok{};
  }
  static result<void> Apply(const RE::TESUniqueIDChangeEvent& event,
                            std::set<RE::FormID>& refresh) {
    const auto oldOwner = IDs().FindReference(event.oldBaseID);
    const IDSolver::NativeKey old{oldOwner, event.oldUniqueID};
    if (!IDs().FindItem(old)) {
      return Ok{};  // ignore untracked or dupe events
    }
    if (!IDs().FindItem(old, event.objectID)) {
      return Err{IDSolver::Err_TransferConflict,
                 "Pair of form ID and unique ID not found"};
    }
    refresh.insert(event.oldBaseID);
    refresh.insert(event.newBaseID);
    // if there's a partial split, assign a new UID to the created stack
    if (auto* source =
            RE::TESForm::LookupByID<RE::TESObjectREFR>(event.oldBaseID);
        source && (event.oldBaseID != event.newBaseID ||
                   event.oldUniqueID != event.newUniqueID)) {
      if (const auto sourceStack =
              FindStack(*source, event.objectID, event.oldUniqueID);
          sourceStack) {
        return Ok{};
      } else {
        return sourceStack.error();
      }
    }
    if (!event.newBaseID || !event.newUniqueID) {
      // if the anchor is lost, just delete it
      IDs().DetachItem(old);
      return Ok{};
    }
    auto* owner = RE::TESForm::LookupByID<RE::TESObjectREFR>(event.newBaseID);
    if (const auto newOwner = Reference(*owner)) {
      if (const auto moved = IDs().MoveItem(
              old, {newOwner.value(), event.newUniqueID}, event.objectID)) {
        return Ok{};
      } else {
        return Err{IDSolver::Err_TransferConflict, "Failed to move items. {}",
                   moved.error()};
      }
    } else {
      return Err{IDSolver::Err_NotFound, "Lookup failed for owner {}. {}",
                 event.newBaseID, newOwner.error()};
    }
  }
  result<void> RefreshInventory(RE::TESObjectREFR& owner, bool createNative) {
    auto& ids = IDs();
    const auto form = owner.GetFormID();
    const auto ownerID = ids.FindReference(form);
    if (!ids.IsLoaded(form, ownerID)) return Ok{};
    ids.UnloadInventory(form);
    if (!owner.GetContainer()) return Ok{};
    auto* changes = owner.GetInventoryChanges(true);
    if (!changes) return Ok{};
    const auto counts = owner.GetInventoryCounts(
        RE::TESObjectREFR::DEFAULT_INVENTORY_FILTER, true);
    std::set<NativeUID> reserved;
    if (changes->entryList)
      for (const auto* entry : *changes->entryList) {
        if (!entry || !entry->extraLists) continue;
        for (const auto* instance : *entry->extraLists) {
          if (instance) {
            if (const auto* native = instance->GetByType<RE::ExtraUniqueID>();
                native && native->uniqueID) {
              // go ahead and fill up the reserved with unique IDs that already
              // exist
              reserved.insert(native->uniqueID);
            }
          }
        }
      }
    std::set<NativeUID> nativeIDs;
    bool needsRefresh = false;
    if (changes->entryList) {
      for (const auto* entry : *changes->entryList) {
        if (!entry || !entry->object || !entry->extraLists) continue;
        FIND_IN(counts, entry->object) {
          if (it->second <= 0) continue;
          for (auto* extraList : *entry->extraLists) {
            if (!extraList || extraList->GetCount() <= 0) continue;
            auto* native = extraList->GetByType<RE::ExtraUniqueID>();
            if (!native && createNative) {
              // turns out all of my previous attempts to set the unique ID
              // were wrong because commonlib has the wrong ID for
              // InventoryChanges::SetUniqueID. I'm going to be a dick though
              // and keep that under wraps until this is out. So I don't forget
              // though, the address is 16147 and the signature is
              // SetUniqueID(ExtraDataList*, TESForm* oldOwner, TESForm* obj)
              NativeUID newUID{};
              for (unsigned attempt = 0; attempt < NATIVE_UID_MAX; ++attempt) {
                const auto maybeNext = changes->GetNextUniqueID();
                if (maybeNext == NULL)
                  return Err{"Native inventory IDs exhausted"};
                if (reserved.insert(maybeNext).second) {
                  newUID = maybeNext;
                  break;
                }
              }
              auto created = std::make_unique<RE::ExtraUniqueID>(form, newUID);
              extraList->Add(created.get());
              native =
                  extraList
                      ->GetByType<RE::ExtraUniqueID>();  // sync just in case
              if (native != created.get()) {
                return Err{"Attaching new ExtraUniqueID failed"};
              }
              created.release();
              changes->changed = true;
              owner.AddChange(RE::TESObjectREFR::ChangeFlags::kInventory);
            }
            if (!native || native->uniqueID == NULL) {
              if (!createNative) {
                needsRefresh = true;
                continue;
              }
            }
            if (native->baseID != form) {
              return Err{"Inventory owner changed, expected {} but got {}",
                         form, native->baseID};
            }
            if (!nativeIDs.insert(native->uniqueID).second) {
              return Err{"Attempted to add duplicate unique ID"};
            }
            if (const auto id = ids.Item(ownerID, native->uniqueID,
                                         entry->object->GetFormID())) {
              ids.LoadItem(form, id.value());
            } else {
              return id.error();
            }
          }
        }
      }
    }
    ids.ReconcileInventory(ownerID, nativeIDs);
    if (needsRefresh) {
      Queue(Refresh{form});
    }
    return Ok{};
  }

  std::mutex mutex_;
  std::deque<Event> pending_;
  size_t gen_{};
  std::optional<Err> failure_;
  bool accepting_{}, scheduled_{}, saving_{}, draining_{};
} g_coordinator{};
}  // namespace

result<uid_t> IDSolver::Allocate() {
  if (next_ == std::numeric_limits<uid_t>::max()) {
    return Err{Err_Generic, "Identity limit reached"};
  }
  dirty_ = true;
  return Ok{next_++};
}

PickupCtx::PickupCtx(const RE::TESObjectREFR* owner,
                     const RE::TESObjectREFR* world, int32_t amount)

    : previous_(g_pickupCtx) {
  if (owner && world && world->GetBaseObject() && amount > 0) {
    this->owner = owner->GetFormID();
    refr = world->GetFormID();
    obj = world->GetBaseObject()->GetFormID();
    count = amount;
    isFull = amount == world->extraList.GetCount();
    if (const auto* native = world->extraList.GetByType<RE::ExtraUniqueID>())
      srcNative = native->uniqueID;
  }
  g_pickupCtx = this;
}
PickupCtx::~PickupCtx() { g_pickupCtx = previous_; }
void OnDropped(std::uint32_t owner, std::uint32_t object, NativeUID native,
               RE::TESObjectREFR* world) {
  if (owner && native && world && world->GetBaseObject() &&
      world->GetBaseObject()->GetFormID() == object)
    g_coordinator.Queue(WorldTransfer{.oldOwner = owner,
                                      .newOwner = 0,
                                      .object = object,
                                      .refr = world->GetFormID(),
                                      .native = native,
                                      .count = 0});
}
void IDSolver::RegisterEvents() {
  auto* events = RE::ScriptEventSourceHolder::GetSingleton();
  events->AddEventSink<RE::TESUniqueIDChangeEvent>(&g_coordinator);
  events->AddEventSink<RE::TESContainerChangedEvent>(&g_coordinator);
  events->AddEventSink<RE::TESFormDeleteEvent>(&g_coordinator);
}
void IDSolver::Reset() { g_coordinator.Reset(); }
void IDSolver::Resume() {
  if (!State::GetSingleton()->GetDb().IsReady()) return;
  g_coordinator.Start();
  // some references exist before the load serialization happens, i.e. menu/load
  // screen nifs and the player
  if (auto* world = RE::TES::GetSingleton()) {
    world->ForEachReference([](RE::TESObjectREFR* refr) {
      if (refr && refr->Is3DLoaded()) BeginTrackingRefr(refr);
      return RE::BSContainer::ForEachResult::kContinue;
    });
  }
  if (auto* player = RE::PlayerCharacter::GetSingleton(); player->Is3DLoaded())
    BeginTrackingRefr(player);
}
result<void> IDSolver::PrepareSave() { return g_coordinator.PrepareSave(); }
void IDSolver::FinishSave() { g_coordinator.FinishSave(); }
result<void> IDSolver::ProcessPending() {
  return g_coordinator.ProcessPending();
}
void IDSolver::BeginTrackingRefr(RE::TESObjectREFR* refr) {
  if (refr)
    g_coordinator.Queue(
        Load{.handle = refr->GetHandle(), .form = refr->GetFormID()});
}
void IDSolver::StopTrackingRefr(RE::FormID form) {
  g_coordinator.Queue(Unload{form});
}

void IDSolver::OnDropped(RE::FormID owner, RE::FormID obj, NativeUID nativeUID,
                         RE::TESObjectREFR* world) {
  if (world && world->GetBaseObject()->GetFormID() == obj) {
    g_coordinator.Queue(
        WorldTransfer{owner, NULL, obj, world->GetFormID(), nativeUID, 0});
  }
}

result<uid_t> IDSolver::ParseUniqueID(const RE::TESObjectREFR* target) {
  if (!target || target->IsDeleted() || !target->GetBaseObject()) {
    return Err{EDE_InvalidArgument, "Invalid target"};
  }
  const auto& ids = State::GetSingleton()->GetIDSolver();
  const auto id = ids.FindReference(target->GetFormID(),
                                    target->GetBaseObject()->GetFormID());
  if (!ids.IsLoaded(target->GetFormID(), id) &&
      !(id && State::GetSingleton()->PreloadReady())) {
    return Err{EDE_Error, "Target not loaded"};
  }
  return Ok{id};
}

static result<uid_t> ParseInventoryItem(const RE::TESObjectREFR* owner,
                                        const RE::TESBoundObject* obj,
                                        const RE::ExtraDataList* instance,
                                        const bool validateOwns) {
  if (!owner || owner->IsDeleted()) {
    return Err{EDE_InvalidArgument, "Invalid owner"};
  }
  if (!obj) {
    return Err{EDE_InvalidArgument, "Invalid object"};
  }
  if (!instance) {
    return Err{EDE_InvalidArgument, "Expected non-null ExtraDataList"};
  }
  if (instance->GetCount() <= 0) {
    return Err{EDE_InvalidArgument, "Stack contains zero items"};
  }
  const auto* native = instance->GetByType<RE::ExtraUniqueID>();
  if (!native) {
    return Err{EDE_InvalidArgument, "Item contains no ExtraUniqueID"};
  }
  if (native->baseID != owner->GetFormID()) {
    // this is only verifying EDE made it
    return Err{EDE_InvalidArgument,
               "Item contains ExtraUniqueID but not from EDE"};
  }
  if (validateOwns) {
    if (const auto* changes =
            const_cast<RE::TESObjectREFR*>(owner)->GetInventoryChanges(true);
        changes && changes->entryList) {
      auto found = false;
      for (const auto* entry : *changes->entryList) {
        if (!entry || entry->object != obj || !entry->extraLists) continue;
        for (const auto* extraList : *entry->extraLists) {
          if (extraList == instance) {
            found = true;
            break;
          }
        }
      }

      if (!found) {
        return Err{EDE_InvalidArgument, "Owner failed validation check"};
      }
    }
    return Err{EDE_InvalidArgument, "Owner contains no inventory"};
  }
  const auto& solver = State::GetSingleton()->GetIDSolver();
  const auto id = solver.FindItem(native->baseID, native->uniqueID);
  if (!solver.IsLoaded(owner->GetFormID(), id) &&
      !(id && State::GetSingleton()->PreloadReady())) {
    return Err{EDE_InvalidArgument, "Reference not loaded"};
  }
  return Ok{id};
}

result<uid_t> IDSolver::ParseUniqueID(const RE::TESObjectREFR* owner,
                                      const RE::ExtraDataList* stack) {
  if (!owner || owner->IsDeleted()) {
    return Err{EDE_InvalidArgument, "Invalid owner"};
  }
  if (!stack) {
    return Err{EDE_InvalidArgument, "Invalid stack"};
  }
  if (const auto* changes =
          const_cast<RE::TESObjectREFR*>(owner)->GetInventoryChanges(true);
      changes && changes->entryList) {
    for (const auto* entry : *changes->entryList) {
      if (!entry || !entry->object || !entry->extraLists) continue;
      for (const auto* extraList : *entry->extraLists) {
        if (extraList == stack) {
          // don't need to validate ownership, we've just done it here
          return ParseInventoryItem(owner, entry->object, stack, false);
        }
      }
    }
  }
  return Err{EDE_InvalidArgument, "Owner failed validation check"};
}

result<uid_t> IDSolver::ParseUniqueID(const RE::TESObjectREFR* owner,
                                      const RE::TESBoundObject* obj,
                                      const RE::ExtraDataList* stack) {
  return ParseInventoryItem(owner, obj, stack, true);
}
}  // namespace ExtraDataExtender
