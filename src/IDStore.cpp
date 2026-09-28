#include "IDStore.h"

#include <fmt/format.h>

#include "State.h"

#define IS_VALID_ID(_ID) ((_ID) != nullptr && std::strlen(_ID) <= MAX_ID)
#define DB_KEY(_KEY) \
  fmt::format("ede/payload/{:016X}/{}", (_KEY).first, (_KEY).second)

namespace ExtraDataExtender {
// this file utilizes error codes a lot more because most of these
// functions are being called through the ABI. Also, I really wanted
// this to be in one file but alas, it needs to access the database
// and I didn't want to manage the headache of passing the db in as
// a ref since it's a unique ptr in the state.
namespace {
constexpr size_t MAX_PAYLOAD = 1024ULL * 1024ULL;
constexpr size_t MAX_ID = 128;

Err ReentryErr() {
  return Err{"PayloadStore duplicate re-entry found"};
}

// should probably change this to use StringWriter/StringReader
class StreamImpl final : public SerializationStream {
 public:
  StreamImpl() = default;
  explicit StreamImpl(const std::string_view input)
      : input_(input), isReading_(true) {}

  bool WriteRecordData(void* buffer, uint32_t length) const override {
    if (isReading_) {
      logger::error("Cannot write to reading stream");
      didFail_ = true;
      return false;
    }
    if (didFail_) {
      logger::error("Cannot write to failed stream");
      return false;
    }
    if (length > 0) {
      if (buffer == nullptr) {
        logger::error("Got non-zero length for a null buffer");
        didFail_ = true;
      } else {
        bytes_.append(static_cast<const char*>(buffer), length);
      }
    }
    return !didFail_;
  }

  uint32_t ReadRecordData(void* buffer, uint32_t length) const override {
    if (!isReading_) {
      logger::error("Cannot read from a writing stream");
      didFail_ = true;
      return NULL;
    }
    if (length > 0 && buffer == nullptr) {
      logger::error("Got non-zero length for a null buffer");
      return NULL;
    }
    const auto count =
        static_cast<uint32_t>(std::min<size_t>(length, input_.size()));
    if (count > 0) {
      std::memcpy(buffer, input_.data(), count);
    }
    input_.remove_prefix(count);
    return count;
  }

  bool DidFail() const { return didFail_; }
  bool AtEnd() const { return !isReading_ || input_.empty(); }
  const std::string& GetBytes() const { return bytes_; }
  size_t GetRemainingCount() const { return input_.size(); }

 private:
  mutable std::string_view input_;
  mutable std::string bytes_;
  mutable bool didFail_{};
  bool isReading_{};
};

struct Envelope {
  uint32_t version;
  std::string_view payload;
};

struct ReentryGuard {
  bool& active;
  explicit ReentryGuard(bool& value) : active(value) { active = true; }
  ~ReentryGuard() { active = false; }
  DONOTMOVEITMOVEIT(ReentryGuard);
};

result<Envelope> TryDecode(std::string_view bytes) {
  try {
    StringReader reader{bytes};
    if (reader.ReadString(4) != "EDEP") {
      return Err{"Invalid payload header"};
    }
    const auto version = reader.ReadUInt();
    const auto length = reader.ReadUInt();
    if (length > MAX_PAYLOAD) {
      return Err{"Payload too large. Maximum size {}, got {}", MAX_PAYLOAD,
                 length};
    }
    if (length != reader.Remaining()) {
      return Err{"Payload length mismatches buffer length. Expected {}, got {}",
                 reader.Remaining(), length};
    }
    return Ok{
        Envelope{.version = version, .payload = reader.ReadStringView(length)}};
  } catch (const std::exception&) {
    return Err{"Truncated string during read"};
  }
}

result<std::string> TryEncode(const Envelope& envelope) {
  if (envelope.payload.size() > MAX_PAYLOAD) {
    return Err{"Payload too large. Maximum size {}, got {}", MAX_PAYLOAD,
               envelope.payload.size()};
  }
  StringWriter writer{};
  writer.Write("EDEP")
      .Write(envelope.version)
      .Write(envelope.payload.size(), 4)
      .Write(envelope.payload);
  return Ok{std::move(writer.Take())};
}

Database& GetDb() { return State::GetSingleton()->GetDb(); }
}  // namespace

result<IDStore::ValueMap::iterator> IDStore::FindValue(
    const Key& key) {
  if (key.first == NULL || !IS_VALID_ID(key.second.c_str()) ||
      !kinds_.contains(key.second)) {
    return Err{"Invalid payload key. {} - {}", key.first, key.second};
  }
  FIND_IN(values_, key) { return Ok{it}; }
  if (const auto stored = GetDb().Contains(DB_KEY(key))) {
    if (stored.value()) {
      
    }
    return Ok{values_
                  .emplace(key, stored.value() ? Value{PresentValue{}}
                                               : Value{MissingValue{}})
                  .first};
  } else {
    return stored.error();
  }
}

result<IDStore::ValueMap::iterator> IDStore::FindPayload(
    const Key& key) {
  if (const auto located = FindValue(key)) {
    const auto it = located.value();
    if (!it->second.holds_alternative<PresentValue>()) {
      return Ok{it};
    }
    if (const auto bytes = GetDb().Read(DB_KEY(key))) {
      it->second.emplace<PersistedValue>(
          PersistedValue{.bytes = bytes.value()});
      return Ok{it};
    } else {
      return bytes.error();
    }
  } else {
    return located.error();
  }
}

result<void> IDStore::Write(
                                 const bool serializeAll) {
  if (isEntered_) {
    return Err{"Can't write during callback"};
  }
  ReentryGuard guard{isEntered_};
  std::vector<Database::Mutation> mutations;
  for (auto& [key, value] : values_) {
    const auto* obj = value.get_if<Obj>();
    if (!obj || (!serializeAll && !removeFromActive_.contains(key.first))) {
      continue;
    }
    StreamImpl stream;
    try {
      if (!(*obj)->Write(&stream) || stream.DidFail()) {
        return Err{"Failed to write payload for {} - {}", key.first,
                   key.second};
      }
    } catch (const std::exception& err) {
      return Err{"Failed to write payload for {} - {}: {}", key.first,
                 key.second, err.what()};
    }
    if (const auto envelope = TryEncode(Envelope{
            kinds_.find(key.second)->second.version, stream.GetBytes()})) {
      mutations.push_back({DB_KEY(key), std::move(envelope).value()});
    } else {
      return envelope.error();
    }
  }
  if (!mutations.empty()) {
    if (const auto saved = GetDb().Write(std::move(mutations)); !saved) {
      return Err{"Failed to commit payload. {}", saved.error()};
    }
  }
  if (!removeFromActive_.empty()) {
    values_.erase_if([&](const ValueMap::value_type& kvp) {
      if (!removeFromActive_.contains(kvp.first.first)) return false;
      if (const auto* obj = kvp.second.get_if<Obj>()) {
        owned_.erase(obj->get());
      }
      return true;
    });
  }
  return Ok{};
}

result<void> IDStore::Register(const char* id, const unsigned version,
                                    const abi::Create ctor,
                                    const abi::Destroy dtor) {
  if (isEntered_) {
    return ReentryErr();
  }
  if (!IS_VALID_ID(id)) {
    return Err{EDE_InvalidArgument, "Invalid ID"};
  }
  if (!ctor) {
    return Err{EDE_InvalidArgument, "Constructor required"};
  }
  if (!dtor) {
    return Err{EDE_InvalidArgument, "Destructor required"};
  }
  if (kinds_.contains(id)) {
    return Err{EDE_DuplicateKind, "Duplicate found with ID {}", id};
  }
  ReentryGuard guard{isEntered_};
  auto* raw = ctor();
  if (!raw) {
    return Err{EDE_InvalidArgument, "Constructor returned null"};
  }
  if (owned_.contains(raw)) {
    return Err{EDE_InvalidArgument, "A constructor must return a unique value"};
  }
  Obj prototype{raw, Destructor{dtor}};
  if (const auto* actual = prototype->GetID();
      !actual || std::string_view(actual) != id) {
    return Err{EDE_InvalidArgument, "ID mismatch found. Expected {} but got {}",
               id, actual};
  }
  if (prototype->GetVersion() != version) {
    return Err{EDE_InvalidArgument,
               "Version mismatch found. Expected {} but got {}", version,
               prototype->GetVersion()};
  }
  kinds_.emplace(id, Kind{.version = version,
                          .ctor = ctor,
                          .dtor = dtor,
                          .type = typeid(*prototype)});
  return Ok{};
}

result<bool> IDStore::Exists(const char* id, const unsigned version) {
  if (isEntered_) return ReentryErr();
  if (!IS_VALID_ID(id)) return Err{EDE_InvalidArgument, "Invalid ID {}", id};
  FIND_IN(kinds_, id) { return Ok{it->second.version == version}; }
  return Ok{false};
}

result<bool> IDStore::Has(uid_t target, const char* id) {
  if (isEntered_) return ReentryErr();
  if (target == NULL) {
    return Err{EDE_InvalidArgument, "Empty target ID"};
  }
  if (!IS_VALID_ID(id)) {
    return Err{EDE_InvalidArgument, "Invalid ID {}"};
  }
  if (const auto result = FindValue({target, id})) {
    return Ok{!result.value()->second.holds_alternative<MissingValue>()};
  }
  return Ok{false};
}

result<bool> IDStore::Add(uid_t target, ExtraData* data) {
  if (isEntered_) return ReentryErr();
  if (target == NULL) {
    return Err{EDE_InvalidArgument, "Empty target ID"};
  }
  if (!data) {
    return Err{EDE_InvalidArgument, "Expected non-null extra data"};
  }
  ReentryGuard guard{isEntered_};
  const auto* id = data->GetID();
  FIND_IN(kinds_, data->GetID()) {
    if (it->second.version != data->GetVersion()) {
      return Err{
          EDE_InvalidArgument,
          "Version mismatch between the registered type and the passed type"};
    }
    if (it->second.type != std::type_index(typeid(*data))) {
      return Err{
          EDE_InvalidArgument,
          "Invalid type definition bound between data and the registered kind"};
    }
    const Key key{target, id};
    if (const auto found = FindValue(key)) {
      if (found.value()->second.holds_alternative<MissingValue>()) {
        owned_.insert(data);
        found.value()->second.emplace<Obj>(data, Destructor{it->second.dtor});
        return Ok{true};
      }
      return Ok{false};
    } else {
      return found.error();
    }
  }
  return Err{EDE_InvalidArgument,
             "Definition for extra data with ID {} not found", data->GetID()};
}

result<ExtraData*> IDStore::Get(uid_t target, const char* id) {
  if (isEntered_) return ReentryErr();
  if (target == NULL) {
    return Err{EDE_InvalidArgument, "Empty target ID"};
  }
  if (!IS_VALID_ID(id)) {
    return Err{EDE_InvalidArgument, "Invalid extra data ID"};
  }
  const Key key{target, id};
  FIND_IN(kinds_, id) {
    if (const auto found = FindPayload(key)) {
      if (const auto* obj = found.value()->second.get_if<Obj>()) {
        return Ok{obj->get()};
      }
      if (auto* persisted = found.value()->second.get_if<PersistedValue>()) {
        if (persisted->didAttempt) {
          return Ok{nullptr};  // just return, wasn't in LMDB
        }
        persisted->didAttempt = true;
        if (const auto env = TryDecode(persisted->bytes)) {
          ReentryGuard guard{isEntered_};
          auto* raw = it->second.ctor();
          // all the checks for unique values or non-null happens in Register
          // so it should be good here, yeh?
          Obj obj{raw, Destructor{it->second.dtor}};
          StreamImpl stream{env.value().payload};
          try {
            if (!obj->Read(&stream, env.value().version) || stream.DidFail()) {
              return Err{EDE_Error, "Failed to parse payload for target {}",
                         target};
            }
            if (!stream.AtEnd()) {
              return Err{EDE_Error,
                         "Dangling bytes found at the end of the serialization "
                         "stream. {} remaining bytes.",
                         stream.GetRemainingCount()};
            }
          } catch (const std::exception& err) {
            return Err{EDE_Error, "Failed to serialize ExtraData payload. {}",
                       err.what()};
          }
          auto [stored, _] = owned_.insert(obj.get());
          found.value()->second = std::move(obj);
          return Ok{*stored};
        } else {
          return Err{EDE_Error, "Malformed payload detected for target {}. {}",
                     target, env.error()};
        }
      }
    }
  }
  return Ok{nullptr};
}

result<bool> IDStore::Remove(uid_t target, const char* id) {
  if (isEntered_) return ReentryErr();
  const Key key{target, id};
  if (const auto found = FindValue(key); found && !found.value()->second.holds_alternative<MissingValue>()) {
    if (const auto opResult = GetDb().Write({{DB_KEY(key), std::nullopt}}); !opResult) {
      return Ok{false};
    }
    ReentryGuard guard{isEntered_};
    if (auto* obj = found.value()->second.get_if<Obj>()) {
      owned_.erase(obj->get());
    }
    values_.erase(found.value());
    return Ok{true};
  } else {
    return found.error();
  }
}

result<void> IDStore::Flush() {
  return Write(true);
}

result<void> IDStore::Sync(const std::set<uid_t>& loaded) {
  std::erase_if(removeFromActive_, [&](const auto uid) { return loaded.contains(uid); });
  for (const auto& [uid, _] : values_ | std::views::keys) {
    if (!loaded.contains(uid)) {
      removeFromActive_.insert(uid);
    }
  }
  if (!removeFromActive_.empty()) {
    return Write(false);
  }
  return Ok{};
}

result<void> IDStore::Reset() {
  if (isEntered_) {
    return Ok{};
  }
  ReentryGuard guard{isEntered_};
  owned_.clear();
  values_.clear();
  removeFromActive_.clear();
  return Ok{};
}
}  // namespace ExtraDataExtender
