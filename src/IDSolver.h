#pragma once

#include <emhash/hash_table7.hpp>

#include "Helpers.h"
#include "StringReader.h"
#include "StringWriter.h"

namespace ExtraDataExtender {
class IDSolver {
  static Err NullParamErr(const std::string_view paramName) {
    return Err{Err_NullValue, "Invalid param ({}), got NULL", paramName};
  }

 public:
  using NativeKey = std::pair<uid_t, NativeUID>;
  using FormResolver = std::function<bool(RE::FormID, RE::FormID&)>;

  enum Error {
    Err_Generic,
    Err_InvalidParam,
    Err_DataMismatch,
    Err_NotFound,
    Err_NullValue,
    Err_TransferConflict,
    Err_RetryableFailure,
  };

  result<uid_t> Reference(RE::FormID form, RE::FormID obj) {
    if (form == NULL) {
      return NullParamErr("form");
    }
    if (obj == NULL) {
      return NullParamErr("obj");
    }
    FIND_IN(refs_, form) {
      if (it->second.obj != obj)
        return Err{Err_DataMismatch,
                   "Base object mismatch, expected {} but got {}", obj,
                   it->second.obj};
      return Ok{it->second.id};
    }
    auto id = Allocate();
    if (id) {
      refs_.emplace(form, UniqueRecord{.id = id.value(), .obj = obj});
      owners_.insert(id.value());
    }
    return id;
  }

  result<uid_t> Item(uid_t owner, NativeUID nativeUID, RE::FormID obj) {
    if (owner == NULL) {
      return NullParamErr("owner");
    }
    if (nativeUID == NULL) {
      return NullParamErr("nativeUID");
    }
    if (obj == NULL) {
      return NullParamErr("obj");
    }
    if (!owners_.contains(owner))
      return Err{Err_NotFound, "Owner with UID {} not found", owner};
    const NativeKey key{owner, nativeUID};
    FIND_IN(items_, key) {
      if (it->second.obj != obj)
        return Err{Err_DataMismatch,
                   "Base object mismatch, expected {} but got {}", obj,
                   it->second.obj};
      return Ok{it->second.id};
    }
    auto id = Allocate();
    if (id) items_.emplace(key, UniqueRecord{.id = id.value(), .obj = obj});
    return id;
  }

  result<void> MoveItem(NativeKey from, NativeKey to, RE::FormID obj) {
    FIND_IN(items_, from) {
      if (it->second.obj != obj) {
        return Err{Err_DataMismatch,
                   "Base object mismatch, expected {} but got {}", obj,
                   it->second.obj};
      }
      if (from == to) return Ok{};
      if (to.first == NULL) {
        return Err{Err_NullValue, "Invalid anchor: uid_t"};
      }
      if (to.second == NULL) {
        return Err{Err_NullValue, "Invalid anchor: native UID"};
      }
      if (!owners_.contains(to.first)) {
        return Err{Err_NotFound, "Unknown owner {}", to.first};
      }
      if (items_.contains(to)) {
        return Err{Err_Generic, "Duplicate UID found"};
      }
      const auto record = it->second;
      items_.emplace(to, record);
      items_.erase(it);
      for (auto& loaded : loaded_) loaded.second.erase(record.id);
      dirty_ = true;
      return Ok{};
    }
    return Err{Err_NotFound, "Item not found with ID {} - {}", from.first,
               from.second};
  }

  result<void> DropItem(NativeKey from, RE::FormID refr, RE::FormID obj) {
    FIND_IN(items_, from) {
      if (refr == NULL) {
        return NullParamErr("refr");
      }
      if (it->second.obj != obj) {
        return Err{Err_DataMismatch,
                   "Unexpected object form ID, expected {} but got {}", obj,
                   it->second.obj};
      }
      if (refs_.contains(refr)) {
        return Err{Err_Generic, "Duplicate reference found for {}", refr};
      }
      const auto id = it->second.id;
      refs_.emplace(refr, UniqueRecord{.id = id, .obj = obj});
      owners_.insert(id);
      items_.erase(it);
      for (auto& loaded : loaded_) loaded.second.erase(id);
      dirty_ = true;
      return Ok{};
    }
    return Err{Err_NotFound, "Item not found with ID {} - {}", from.first,
               from.second};
  }

  result<void> PickupReference(RE::FormID refr, NativeKey to, RE::FormID obj) {
    FIND_IN(refs_, refr) {
      if (it->second.obj != obj) {
        return Err{Err_DataMismatch,
                   "Unexpected object form ID, expected {} but got {}", obj,
                   it->second.obj};
      }
      if (to.second == NULL) {
        return Err{Err_NullValue, "Invalid anchor: uid_t"};
      }
      if (!owners_.contains(to.first)) {
        return Err{Err_NotFound, "Invalid anchor: owner UID {}", to.first};
      }
      if (items_.contains(to)) {
        return Err{Err_Generic, "Duplicate reference found for {}", refr};
      }
      const auto id = it->second.id;
      if (const auto child = items_.lower_bound({id, 0});
          child != items_.end() && child->first.first == id)
        return Err{Err_Generic,
                   "Pickup target has an active inventory. This shouldn't be "
                   "possible but you figured out how. Cool!"};
      items_.emplace(to, UniqueRecord{.id = id, .obj = obj});
      refs_.erase(it);
      owners_.erase(id);
      loaded_.erase(refr);
      dirty_ = true;
      return Ok{};
    }
    return Err{"refr not found: {}", refr};
  }

  _NODISCARD uid_t FindReference(RE::FormID form, RE::FormID obj = 0) const {
    FIND_IN(refs_, form) {
      return it->second.obj != obj ? NULL : it->second.obj;
    }
    return NULL;
  }

  _NODISCARD uid_t FindItem(NativeKey key, RE::FormID obj = 0) const {
    FIND_IN(items_, key) {
      return it->second.obj != obj ? NULL : it->second.obj;
    }
    return NULL;
  }

  _NODISCARD uid_t FindItem(RE::FormID owner, NativeUID nativeUID) const {
    if (const auto refrUID = FindReference(owner)) {
      return FindItem({refrUID, nativeUID});
    }
    return NULL;
  }

  void ForgetReference(const RE::FormID form) {
    loaded_.erase(form);
    FIND_IN(refs_, form) {
      const auto id = it->second.id;
      ReconcileInventory(id, {});
      refs_.erase(it);
      owners_.erase(id);
      dirty_ = true;
    }
  }

  void DetachItem(const NativeKey key) {
    FIND_IN(items_, key) {
      for (auto& loaded : loaded_) loaded.second.erase(it->second.id);
      items_.erase(it);
      dirty_ = true;
    }
  }

  void ReconcileInventory(uid_t owner, const std::set<NativeUID>& present) {
    auto it = items_.lower_bound({owner, 0});
    while (it != items_.end() && it->first.first == owner) {
      if (present.contains(it->first.second)) {
        ++it;
        continue;
      }
      const auto id = it->second.id;
      for (auto& loaded : loaded_) loaded.second.erase(id);
      it = items_.erase(it);
      dirty_ = true;
    }
  }

  void LoadReference(RE::FormID form) {
    if (refs_.contains(form)) {
      loaded_.emplace(form, std::set<uid_t>{});
    }
  }

  void LoadItem(RE::FormID owner, uid_t item) {
    FIND_IN(loaded_, owner) { it->second.insert(item); }
  }

  void UnloadInventory(RE::FormID owner) {
    FIND_IN(loaded_, owner) { it->second.clear(); }
  }

  void UnloadReference(RE::FormID form) { loaded_.erase(form); }

  bool IsLoaded(RE::FormID owner, uid_t id) const {
    FIND_IN(loaded_, owner) {
      const auto refr = refs_.find(owner);
      return refr->second.id == id || it->second.contains(id);
    }
    return false;
  }

  _NODISCARD std::set<uid_t> LoadedIDs() const {
    std::set<uid_t> result;
    for (const auto& kvp : loaded_) {
      FIND_IN(refs_, kvp.first) { result.insert(it->second.id); }
      result.insert(kvp.second.begin(), kvp.second.end());
    }
    return result;
  }

  _NODISCARD std::set<RE::FormID> Forms() const {
    std::set<RE::FormID> result;
    for (const auto& kvp : refs_) {
      result.insert(kvp.first);
      result.insert(kvp.second.obj);
    }
    for (const auto& [_, obj] : items_ | std::views::values) {
      result.insert(obj);
    }
    return result;
  }

  void ClearLoaded() { loaded_.clear(); }

  void Clear() { *this = IDSolver{}; }

  std::string Encode() const {
    StringWriter writer;
    writer.Write("EDEI")
        .Write(1, 4)
        .Write(next_)
        .Write(refs_.size())
        .Write(items_.size());
    for (const auto& kvp : refs_) {
      writer.Write(kvp.first)
          .Write(kvp.second.id)
          .Write(
              kvp.second.obj);  // sue me, I got tired of writing `writer.Write`
    }
    for (const auto& [key, record] : items_) {
      writer.Write(key.first)
          .Write(key.second)
          .Write(record.id)
          .Write(record.obj);
    }
    return std::move(writer.ToString());
  }

  result<void> Decode(std::string_view bytes) {
    IDSolver replacement;
    StringReader reader{bytes};
    if (reader.ReadString(4) != "EDEI") {
      return Err{Err_DataMismatch, "Invalid identity header"};
    }
    if (const auto schemaVersion = reader.ReadByte(); schemaVersion == 1) {
      replacement.next_ = reader.ReadULong();
      const auto refrCount = reader.ReadULong();
      const auto itemCount = reader.ReadULong();
      if (replacement.next_ == NULL) {
        return Err{Err_NullValue,
                   "Got invalid registry next value, expected non-null"};
      }
      std::set<uid_t> ids;
      const auto isValidID = [replacement, &ids](const uid_t id) {
        return id != NULL && id < replacement.next_ && ids.insert(id).second;
      };
      for (size_t i = 0; i < refrCount; ++i) {
        const auto form = reader.ReadUInt();
        const auto id = reader.ReadULong();
        const auto obj = reader.ReadUInt();
        if (form == NULL || obj == NULL || !isValidID(id)) {
          return Err{Err_Generic, "Invalid reference record ({}, {}, {})", form,
                     id, obj};
        }
        replacement.owners_.insert(id);
      }
      for (size_t i = 0; i < itemCount; ++i) {
        const auto owner = reader.ReadULong();
        const auto nativeUID = reader.ReadUShort();
        const auto id = reader.ReadULong();
        const auto obj = reader.ReadUInt();
        if (replacement.owners_.contains(owner)) {
          return Err{Err_Generic, "Duplicate owner found: {}", owner};
        }
        if (nativeUID == NULL || obj == NULL || !isValidID(id)) {
          return Err{Err_Generic, "Invalid item record ({}, {}, {})", nativeUID,
                     id, obj};
        }
        if (!replacement.items_
                 .try_emplace(NativeKey{owner, nativeUID},
                              UniqueRecord{.id = id, .obj = obj})
                 .second) {
          return Err{Err_Generic, "Failed to insert item record"};
        }
      }
    } else {
      return Err{Err_NotFound, "Unknown schema version {}", schemaVersion};
    }
    *this = std::move(replacement);
    return Ok{};
  }

  result<void> RemapForms(const FormResolver& resolve) {
    IDSolver replacement;
    replacement.next_ = next_;
    for (const auto& kvp : refs_) {
      RE::FormID current{}, obj{};
      if (!resolve(kvp.first, current) || current == NULL ||
          !resolve(kvp.second.obj, obj) || obj == NULL) {
        continue;  // skip, no change found
      }
      if (!replacement.refs_
               .try_emplace(current,
                            UniqueRecord{.id = kvp.second.id, .obj = obj})
               .second) {
        return Err{Err_Generic, "Failed to insert remapped form ID {}",
                   current};
      }
    }
    replacement.dirty_ = true;
    *this = std::move(replacement);
    return Ok{};
  }
  bool Dirty() const { return dirty_; }
  void MarkClean() { dirty_ = false; }

 private:
  struct UniqueRecord {
    uid_t id;
    RE::FormID obj;
  };
  result<uid_t> Allocate();
  uid_t next_ = 1;
  emhash7::HashMap<RE::FormID, UniqueRecord> refs_;
  emhash7::HashMap<RE::FormID, std::set<uid_t>> loaded_;
  std::set<uid_t> owners_;
  std::map<NativeKey, UniqueRecord> items_;
  bool dirty_ = false;
};

class PickupCtx {
 public:
  PickupCtx(const RE::TESObjectREFR* owner, const RE::TESObjectREFR* world,
            int32_t amount);
  ~PickupCtx();

  DONOTMOVEITMOVEIT(PickupCtx);

  RE::FormID owner{}, refr{}, obj{};
  NativeUID srcNative{}, destNative{};
  int32_t count{};
  bool isFull{};

 private:
  PickupCtx* previous_{};
};

void RegisterEvents();
void OnDropped(RE::FormID owner, RE::FormID obj, NativeUID nativeUID,
               RE::TESObjectREFR* world);
void Reset();
void Resume();
result<void> PrepareSave();
void FinishSave();
result<void> ProcessPending();
void BeginTrackingRefr(RE::TESObjectREFR* refr);
void StopTrackingRefr(RE::FormID form);

result<uid_t> ParseUniqueID(const RE::TESObjectREFR* target);
result<uid_t> ParseUniqueID(const RE::TESObjectREFR* owner,
                            const RE::TESBoundObject* obj,
                            const RE::ExtraDataList* stack);
result<uid_t> ParseUniqueID(const RE::TESObjectREFR* owner,
                            const RE::ExtraDataList* stack);
}  // namespace ExtraDataExtender
