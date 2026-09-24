#pragma once

#include <typeindex>
#include <unordered_set>

#include "ExtraDataExtender.h"
#include "Helpers.h"
#include "Save.h"

// could I have split this out into a header and source file? Sure. But I'm lazy
// so no
namespace ExtraDataExtender {
class TypeRegistry {
  static constexpr size_t MAX_PAYLOAD = 1024ULL * 1024ULL;
  static constexpr std::string EDEP = "EDEP";

  struct RegisteredType {
    unsigned version;
    abi::Create ctor;
    abi::Destroy dtor;
    std::type_index type;
  };

  template <class _Self, typename _Dtor>
  struct InvocableDtor {
    _Dtor dtor;
    void operator()(_Self* _this) const noexcept {
      if (_this) dtor(_this);
    }
  };
  using InvItemDtor = InvocableDtor<ExtraData*, abi::Destroy>;
  using Obj = std::unique_ptr<ExtraData, InvItemDtor>;
  using Key = std::pair<uid_t, std::string>;
  struct Slot {
    Obj obj{nullptr, InvItemDtor{}};
    std::optional<std::string> stored{std::nullopt};
    bool didTry{false};
  };

  static constexpr bool IsValidID(const std::string_view id) {
    return !id.empty() && id.size() < 128;
  }

  static void AppendInt(std::string& bytes, uint32_t value) {
    for (unsigned i = 0; i < sizeof(value); ++i) {
      bytes.push_back(static_cast<char>(value & 255));
      value >>= 8;
    }
  }

  static uint32_t ReadInt(const std::string_view bytes) {
    unsigned result = 0;
    for (unsigned i = 0; i < sizeof(uint32_t); ++i) {
      result |= uint32_t{static_cast<unsigned char>(bytes[i])} << (8 * i);
    }
    return result;
  }

  class SerializationStreamImpl : public SerializationStream {
   public:
    SerializationStreamImpl() = default;
    explicit SerializationStreamImpl(const std::string_view stream)
        : input_(stream), reading_(true) {}

    _NODISCARD bool WriteRecordData(const void* buffer,
                                    uint32_t length) const override {
      if (reading_ || failed_ || (length && !buffer) ||
          length > MAX_PAYLOAD - bytes_.size()) {
        failed_ = true;
        return false;
      }
      if (length) bytes_.append(static_cast<const char*>(buffer), length);
      return true;
    }
    _NODISCARD uint32_t ReadRecordData(void* buffer,
                                       uint32_t length) const override {
      if (!reading_ || (length && !buffer)) {
        failed_ = true;
        return 0;
      }
      const auto count =
          static_cast<uint32_t>(std::min<size_t>(length, input_.size()));
      if (count) {
        std::memcpy(buffer, input_.data(), count);
      }
      input_.remove_prefix(count);
      return count;
    }
    bool DidFail() const { return failed_; }
    const std::string& GetBytes() const { return bytes_; }

   private:
    mutable std::string_view input_;
    mutable std::string bytes_;
    mutable bool failed_{false};
    bool reading_{false};
  };

 public:
  ~TypeRegistry() { Reset().assert_ok(); }

  result<EDE_StatusCode> Register(const char* id, const unsigned version,
                                  const abi::Create ctor,
                                  const abi::Destroy dtor) noexcept {
    if (!IsGameThread()) return Err{"Must be called from game thread"};
    if (!IsValidID(id)) {
      return Err{EDE_InvalidArgument, "Invalid ID"};
    }
    if (!ctor) {
      return Err{EDE_InvalidArgument, "No constructor provided"};
    }
    if (!dtor) {
      return Err{EDE_InvalidArgument, "No destructor provided"};
    }
    if (registered_.contains(id))
      return Err{EDE_DuplicateKind, "Duplicate extra data type found"};
    auto* raw = ctor();
    if (owned_.contains(raw))
      return Err{EDE_InvalidArgument, "Registered type already owned"};
    Obj prototype{raw, InvItemDtor{dtor}};
    if (const auto* actual = prototype->GetID();
        !actual || std::string_view{actual} != id ||
        prototype->GetVersion() != version) {
      return Err{EDE_InvalidArgument,
                 "ID or version mismatch in implementation vs Register call"};
    }
    registered_.emplace(id, RegisteredType{.version = version,
                                           .ctor = ctor,
                                           .dtor = dtor,
                                           .type = typeid(*prototype)});
    return Ok{EDE_Ok};
  }
  result<bool> Exists(const char* id, const unsigned version) const noexcept {
    if (!IsGameThread()) return Err{"Must be called from game thread"};
    if (!IsValidID(id)) return Ok{false};
    const auto it = registered_.find(id);
    return Ok{it != registered_.end() && it->second.version == version};
  }

  result<bool> Has(uid_t target, const char* id) noexcept {
    if (!IsGameThread()) return Err{"Must be called from game thread"};
    if (const auto result = Find(target, id)) {
      return Ok{result->obj && result->stored.has_value()};
    } else {
      return result.error();
    }
  }

  result<ExtraData*> Get(uid_t target, const char* id) noexcept {
    if (!IsGameThread()) return Err{"Must be called from game thread"};
    if (auto slot = Find(target, id)) {
      if (slot->obj || !slot->stored || slot->didTry) {
        return Ok{slot->obj.get()};
      }
      slot->didTry = true;
      const auto& bytes = *slot->stored;
      if (bytes.size() < 12 || !bytes.starts_with(EDEP) ||
          bytes.size() - 12 > MAX_PAYLOAD ||
          ReadInt(bytes.substr(8, 4)) != bytes.size() - 12) {
        return Err{"Preserved payload malformed {} on {}", id, target};
      }
      const auto& kind = registered_.at(id);
      auto* raw = kind.ctor();
      if (owned_.contains(raw)) return Err{"Already owned"};
      Obj obj{raw, InvItemDtor{kind.dtor}};
      SerializationStreamImpl stream{bytes.substr(12)};
      try {
        // god I hate try/catch
        if (kind.type != std::type_index(typeid(*obj)) ||
            !obj->Read(&stream, ReadInt(bytes.substr(4, 4))) ||
            stream.DidFail()) {
          return Err{"Unsupported payload preserved for target {} with ID {}",
                     target, id};
        }
      } catch (std::runtime_error& err) {
        return Err{"Stream read failed for target {} with ID {}: {}", target,
                   id, err.what()};
      }
      owned_.insert(obj.get());
      slot->obj = std::move(obj);
      return Ok{slot->obj.get()};
    } else {
      return slot.error();
    }
  }

  result<bool> Add(uid_t target, ExtraData* data) {
    if (!IsGameThread()) return Err{"Must be called from game thread"};
    const auto* id = data->GetID();
    if (!IsValidID(id)) {
      return Err{"Invalid ID"};
    }
    const auto registered = registered_.find(id);
    if (registered == registered_.end() ||
        registered->second.type != std::type_index(typeid(*data)) ||
        data->GetVersion() != registered->second.version) {
      return Err{"Unexpected ExtraData kind"};
    }
    if (const auto slot = Find(target, id)) {
      if (!slot.value() || slot->obj || slot->stored) {
        return Ok{false};
      }
      owned_.insert(data);
      slot->obj = Obj{data, InvItemDtor{registered->second.dtor}};
      slot->didTry = true;
      return Ok{true};
    } else {
      return slot.error();
    }
  }

  result<bool> Remove(uid_t target, const char* id) {
    if (!IsGameThread()) return Err{"Must be called from game thread"};
    if (!Has(target, id)) return Ok{false};
    auto& db = Save::GetSingleton()->GetDb();
    const Key key{target, id};
    if (const auto removed = db.Write({{GetDbKey(key), std::nullopt}});
        !removed) {
      return Ok{false};
    }

    owned_.erase(slots_.at(key).obj.get());
    slots_.erase(key);
    return Ok{true};
  }

  result<bool> Flush() { return Write(nullptr); }

  void SynchronizeLoaded(const std::set<uid_t>& loaded) {
    std::set<uid_t> toRemove;
    for (const auto& [uid, _] : slots_ | std::views::keys) {
      if (!loaded.contains(uid)) toRemove.insert(uid);
    }
    if (const auto result = Write(&toRemove); !result || !result.value()) {
      logger::warn("Failed to remove prior loaded IDs: {}",
                   result.match<std::string>(
                       [](bool) { return "Write returned false"; },
                       [](const Err& err) { return err.message; }));
    }
    for (const auto uid : loaded) {
      for (const auto& id : registered_ | std::views::keys) {
        if (const auto writeResult = Get(uid, id.c_str()); !writeResult) {
          logger::error("Failed to preload UID {}: {}", uid,
                        writeResult.error());
        }
      }
    }
  }

  result<bool> Reset() {
    if (!IsGameThread()) return Err{"Must be called from game thread"};
    owned_.clear();
    slots_.clear();
    return Ok{true};
  }

 private:
  result<Slot*> Find(uid_t target, const char* id) {
    if (target == NULL) return Err{"Invalid target"};
    if (!IsValidID(id)) return Err{"Invalid ID"};
    if (!registered_.contains(id))
      return Err{"Registered type not found for ID {}", id};
    const Key key{target, id};
    if (const auto it = slots_.find(key); it != slots_.end())
      return Ok{&it->second};
    auto& db = Save::GetSingleton()->GetDb();
    const auto bytes = db.Read(GetDbKey(key));
    if (!bytes) {
      return Err{"EDE payload read failed: {}", bytes.error()};
    }
    Slot slot;
    slot.stored = bytes.value();
    return Ok{&slots_.emplace(key, std::move(slot)).first->second};
  }

  result<bool> Write(const std::set<uid_t>* toRemove) {
    if (!IsGameThread()) return Err{"Must be called from game thread"};
    std::vector<LMDB::Connection::Mutation> mutations;
    for (auto& [key, slot] : slots_) {
      if (!slot.obj || (toRemove && !toRemove->contains(key.first))) continue;
      SerializationStreamImpl stream;
      try {
        if (!slot.obj->Write(&stream) || stream.DidFail()) {
          return Err{"Write failed for {} on {}, early exiting write",
                     key.second, key.first};
        }
      } catch (std::runtime_error& err) {
        return Err{"Write failed for {} on {} ({}), early exiting write",
                   key.second, key.first, err.what()};
      }
      std::string bytes = EDEP;
      AppendInt(bytes, registered_.at(key.second).version);
      AppendInt(bytes, static_cast<uint32_t>(stream.GetBytes().size()));
      bytes += stream.GetBytes();
      mutations.push_back({GetDbKey(key), std::move(bytes)});
    }
    if (!mutations.empty()) {
      const auto& db = Save::GetSingleton()->GetDb();
      if (const auto saved = db.Write(std::move(mutations)); !saved) {
        return saved.error();
      }
    }
    if (toRemove) {
      std::erase_if(slots_, [&](const decltype(slots_)::value_type& kvp) {
        if (!toRemove->contains(kvp.first.first)) return false;
        owned_.erase(kvp.second.obj.get());
        return true;
      });
    }
    return Ok{true};
  }

  static std::string GetDbKey(const Key& key) {
    return fmt::format("ede/payload/{:016x}/{}", key.first, key.second);
  }

  std::unordered_map<std::string, RegisteredType> registered_;
  std::unordered_map<Key, Slot> slots_;
  std::unordered_set<ExtraData*> owned_;
};
}  // namespace ExtraDataExtender