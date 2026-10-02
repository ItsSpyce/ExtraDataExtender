#pragma once

#include <emhash/hash_table8.hpp>
#include <typeindex>
#include <unordered_set>

#include "ExtraDataExtender.h"
#include "UniqueID.h"

namespace ExtraDataExtender {
class IDStore final {
  struct Kind {
    unsigned version;
    abi::Create ctor;
    abi::Destroy dtor;
    abi::IsNotEqual isNotEqual;
    std::type_index type;
  };

  struct Destructor {
    abi::Destroy destroy{};
    void operator()(ExtraData* data) const noexcept {
      if (data) destroy(data);
    }
  };
  using Obj = std::unique_ptr<ExtraData, Destructor>;
  using Key = std::pair<uid_t, std::string>;
  struct KeyHash {
    std::size_t operator()(const Key& key) const noexcept {
      const auto first = std::hash<uid_t>{}(key.first);
      const auto second = std::hash<std::string>{}(key.second);
      return first ^ second + 0x9e3779b9 + (first << 6) + (first >> 2);
    }
  };
  struct MissingValue {};
  struct PresentValue {};
  struct PersistedValue {
    std::string bytes;
    bool didAttempt = false;
  };
  using Value = stl::better_variant<MissingValue, PresentValue, PersistedValue, Obj>;
  using ValueMap = emhash8::HashMap<Key, Value, KeyHash>;

 public:
  result<void> Register(const char* id, unsigned version, abi::Create ctor,
                        abi::Destroy dtor, abi::IsNotEqual isNotEqual);

  result<bool> Exists(const char* id, unsigned version);
  result<bool> Has(uid_t target, const char* id);
  result<bool> Add(uid_t target, ExtraData* data);
  result<ExtraData*> Get(uid_t target, const char* id);
  result<bool> Remove(uid_t target, const char* id);
  result<void> Flush();
  result<void> Sync(const std::set<uid_t>& loaded);
  result<void> Reset();

  bool NeedsSync() const { return !removeFromActive_.empty(); }
  bool IsEntered() const { return isEntered_; }

 private:
  emhash8::HashMap<std::string, Kind> kinds_;
  ValueMap values_;
  std::unordered_set<ExtraData*> owned_;
  std::set<uid_t> removeFromActive_;
  bool isEntered_{false};

  /// <summary>
  ///  Searches for the key without loading data from LMDB
  /// </summary>
  /// <param name="key"></param>
  /// <returns></returns>
  result<ValueMap::iterator> FindState(const Key& key);
  /// <summary>
  ///   Searches for the key and loads data from LMDB
  /// </summary>
  /// <param name="key"></param>
  /// <returns></returns>
  result<ValueMap::iterator> FindAndLoad(const Key& key);

  result<void> Write(bool serializeAll);
};
}  // namespace ExtraDataExtender
