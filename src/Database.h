#pragma once

#include <lmdb.h>

#include "Fsp.h"

namespace ExtraDataExtender {
class Database {
  using epoch_t = uint64_t;
  using Env = std::unique_ptr<MDB_env, decltype(&mdb_env_close)>;
  using Txn = std::unique_ptr<MDB_txn, decltype(&mdb_txn_abort)>;

  static constexpr char SCHEMA_VERSION[] = {1, 0, 0, 0};
  static constexpr size_t INITIAL_MAP_SIZE = 64ULL * 1024;           // 64kb
  static constexpr size_t MAX_MAP_SIZE = 1ULL * 1024 * 1024 * 1024;  // 1gb

  static void assert_lmdb_ok(const int rc) {
    if (MDB_SUCCESS != rc) {
      throw std::runtime_error(mdb_strerror(rc));
    }
  }

  static result<void> check_lmdb_ok(const int rc) {
    if (MDB_SUCCESS != rc) {
      return Err{rc, "Got error from LMDB: {}", mdb_strerror(rc)};
    }
    return Ok{};
  }

  static Env OpenLMDB(const fs::path& path, size_t mapBytes, bool isReadonly) {
    MDB_env* ptr{};
    assert_lmdb_ok(mdb_env_create(&ptr));
    Env env{ptr, mdb_env_close};
    assert_lmdb_ok(mdb_env_set_maxdbs(ptr, 2));
    assert_lmdb_ok(mdb_env_set_mapsize(ptr, mapBytes));
    assert_lmdb_ok(mdb_env_open(ptr, path.string().c_str(),
                                isReadonly ? MDB_RDONLY | MDB_NOLOCK : 0,
                                0600));
    return env;
  }

  static MDB_val View(const std::string& bytes) {
    return {.mv_size = bytes.size(),
            .mv_data = const_cast<char*>(bytes.data())};
  }

  static Txn BeginTxn(MDB_env* env, bool isReadonly = false) {
    MDB_txn* txn{};
    assert_lmdb_ok(
        mdb_txn_begin(env, nullptr, isReadonly ? MDB_RDONLY : 0, &txn));
    return Txn{txn, mdb_txn_abort};
  }

  static void CommitTxn(Txn& txn) {
    assert_lmdb_ok(mdb_txn_commit(txn.release()));
  }

  static result<void> GetTables(MDB_env* env, bool create, MDB_dbi& meta,
                                MDB_dbi& data) {
    auto txn = BeginTxn(env, !create);
    if (const auto rc = check_lmdb_ok(
            mdb_dbi_open(txn.get(), "meta", create ? MDB_CREATE : 0, &meta));
        !rc) {
      return rc.error();
    }
    if (const auto rc = check_lmdb_ok(
            mdb_dbi_open(txn.get(), "data", create ? MDB_CREATE : 0, &data));
        !rc) {
      return rc.error();
    }
    auto key = View("schema");
    MDB_val val{};
    if (const auto rc = mdb_get(txn.get(), meta, &key, &val);
        rc == MDB_NOTFOUND && create) {
      val = {.mv_size = sizeof(SCHEMA_VERSION),
             .mv_data = const_cast<char*>(SCHEMA_VERSION)};
      if (const auto putR =
              check_lmdb_ok(mdb_put(txn.get(), meta, &key, &val, 0));
          !putR) {
        return putR.error();
      }
    } else {
      if (const auto rcR = check_lmdb_ok(rc); !rcR) {
        return rcR.error();
      }
      if (std::memcmp(val.mv_data, SCHEMA_VERSION, sizeof(SCHEMA_VERSION)) !=
          0) {
        return Err{"Unsupported db schema"};
      }
    }
    CommitTxn(txn);
    return Ok{};
  }

  static result<size_t> GetEnvMapSize(MDB_env* env) {
    MDB_envinfo info{};
    if (const auto rc = check_lmdb_ok(mdb_env_info(env, &info)); !rc) {
      return Err{"Failed to get LMDB map size: {}", rc.error()};
    }
    return Ok{info.me_mapsize};
  }

  static bool IsValidSaveName(const std::string& name) {
    return !name.empty() && name != "." && name != ".." &&
           !name.contains('/') && !name.contains('\\');
  }

  struct Instance {
    Env env{nullptr, mdb_env_close};
    fs::path dir;
    MDB_dbi meta{}, data{};

    Instance(const fs::path& dir) : dir(std::move(dir)) {}
    ~Instance() { Close(); }
    DONOTMOVEITMOVEIT(Instance);

    void Close() noexcept {
      env.reset();
      fsp::rm_dir(dir);
      dir.clear();
    }

    result<bool> Grow() const noexcept {
      auto [currentMapSize, err] = GetEnvMapSize(env.get()).into_options();
      if (err) {
        return Err{"Failed to read LMDB env info. {}", *err};
      }
      if (currentMapSize >= MAX_MAP_SIZE) return Ok{false};
      const auto size = std::min(*currentMapSize * 2, MAX_MAP_SIZE);
      if (const auto rc = check_lmdb_ok(mdb_env_set_mapsize(env.get(), size));
          !rc) {
        return Err{"Failed to set map size. {}", rc.error()};
      }
      return Ok{true};
    }

    result<void> Start(const std::string& saveName) noexcept {
      auto mapSize = INITIAL_MAP_SIZE;
      fsp::TempDir work{fsp::reinit_dir(dir / ".temp")};
      if (!saveName.empty()) {
        const auto sourcePath = dir / "saves" / saveName;
        auto source = OpenLMDB(sourcePath, INITIAL_MAP_SIZE, true);
        MDB_dbi sourceMeta{}, sourceData{};
        if (const auto r =
                GetTables(source.get(), false, sourceMeta, sourceData);
            !r) {
          return r.error();
        }
        if (const auto currentMapSize = GetEnvMapSize(env.get())) {
          mapSize = std::max(mapSize, *currentMapSize);
          if (mapSize > MAX_MAP_SIZE) {
            return Err{"Database full"};
          }
          if (const auto rc = check_lmdb_ok(
                  mdb_env_copy2(source.get(), work.path.string().c_str(), 0));
              !rc) {
            return Err{"Failed to copy prior save to active work directory"};
          }
        } else {
          return currentMapSize.error();
        }
      }
      env = OpenLMDB(work.path, mapSize, false);
      dir = work.GetAndRelease();
      try {
        for (;;) {
          if (const auto rc =
                  GetTables(env.get(), saveName.empty(), meta, data);
              !rc) {
            if (rc.error().code == MDB_MAP_FULL) {
              if (const auto grow = Grow(); !grow) {
                return Err{"Database full, failed to grow. {}", grow.error()};
              } else if (!grow.value()) {
                return Err{"Database reached max size"};
              }
            } else {
              return rc.error();
            }
          }
          return Ok{};
        }
      } catch (std::exception& err) {
        Close();
        return Err{"An error occured while loading the previous save. {}",
                   err.what()};
      }
    }

    result<void> Save(const std::string& name) const {
      if (const auto rc = check_lmdb_ok(mdb_env_sync(env.get(), 1)); !rc) {
        return Err{"Failed to sync save {}. {}", name, rc.error()};
      }
      fs::create_directories(dir / "saves");
      fsp::TempDir pending{fsp::reinit_dir(dir / ".pending")};
      if (const auto rc = check_lmdb_ok(
              mdb_env_copy2(env.get(), pending.path.string().c_str(), 0));
          !rc) {
        return Err{"Failed to copy save to working directory. {}", rc.error()};
      }
      {
        const auto currentMapSize = GetEnvMapSize(env.get());
        if (currentMapSize.error()) {
          return Err{"Failed to get save size. {}", currentMapSize.error()};
        }
        auto saveNameInstance = OpenLMDB(pending.path, *currentMapSize, true);
        MDB_dbi copiedMeta{}, copiedData{};
        if (const auto rc = GetTables(env.get(), false, copiedMeta, copiedData);
            !rc) {
          return Err{"Failed to get save tables. {}", rc.error()};
        }
      }
      if (const auto result = fsp::flush_file(pending.path / "data.mdb");
          !result) {
        return Err{"Failed to flush save's temp directory. {}", result.error()};
      }
      if (const auto result = fsp::replace_file(
              pending.path, dir / "saves" / name, dir / ".backup");
          !result) {
        return Err{"Failed to replace save contents with working directory. {}",
                   result.error()};
      }
      pending.Release();
      return Ok{};
    }
  };

 public:
  using Mutation = std::pair<std::string, std::optional<std::string>>;

  DONOTMOVEITMOVEIT(Database);
  Database(const fs::path& root) : root_(std::move(root)) {
    fsp::rm_dir(root / ".temp");
    fsp::rm_dir(root / ".pending");
    fsp::rm_dir(root / ".backup");
    instance_ = std::make_unique<Instance>(root);
  }
  ~Database() { Shutdown(); }

  result<void> Write(const std::vector<Mutation>& changes) noexcept {
    if (!ready_) return Err{"EDE not ready"};
    auto* env = instance_->env.get();
    const auto txn = BeginTxn(env, false);
    for (const auto& [key, value] : changes) {
      if (key.empty() || key.size() > mdb_env_get_maxkeysize(env)) {
        return Err{"Invalid key. {}", key};
      }
      auto lmdbKey = View(key);
      if (value.has_value()) {
        auto lmdbValue = View(*value);
        if (const auto putResult = check_lmdb_ok(
                mdb_put(txn.get(), instance_->data, &lmdbKey, &lmdbValue, 0));
            !putResult) {
          if (putResult.error().code != MDB_MAP_FULL || !instance_->Grow()) {
            ready_ = false;
            return Err{"Failed to put record into database. {}",
                       putResult.error()};
          }
          // try it again :D
          if (!check_lmdb_ok(mdb_put(txn.get(), instance_->data, &lmdbKey,
                                     &lmdbValue, 0))) {
            ready_ = false;
            return Err{"Failed to put record into database after growing"};
          }
        }
      } else {
        if (const auto delResult = check_lmdb_ok(
                mdb_del(txn.get(), instance_->data, &lmdbKey, nullptr));
            !delResult) {
          return Err{"Failed to delete record from database. {}",
                     delResult.error()};
        }
      }
    }
    return Ok{};
  }

  result<bool> Contains(const std::string& key) const {
    if (!ready_) return Err{"EDE not ready"};
    auto* env = instance_->env.get();
    if (key.empty() || key.size() > mdb_env_get_maxkeysize(env)) {
      return Err{"Invalid key. {}", key};
    }
    auto txn = BeginTxn(env, true);
    auto lmdbKey = View(key);
    MDB_val value{};
    if (const auto rc = check_lmdb_ok(
            mdb_get(txn.get(), instance_->data, &lmdbKey, &value));
        !rc) {
      if (rc.error().code == MDB_NOTFOUND) {
        return Ok{false};
      }
      return Err{"Failed to check if database contains key {}. {}", key,
                 rc.error()};
    }
    return Ok{true};
  }

  result<std::string> Read(const std::string& key) const {
    if (!ready_) return Err{"EDE not ready"};
    auto* env = instance_->env.get();
    if (key.empty() || key.size() > mdb_env_get_maxkeysize(env)) {
      return Err{"Invalid key. {}", key};
    }
    const auto txn = BeginTxn(env, true);
    auto lmdbKey = View(key);
    MDB_val value{};
    if (const auto rc = check_lmdb_ok(
            mdb_get(txn.get(), instance_->data, &lmdbKey, &value));
        !rc) {
      if (rc.error().code == MDB_NOTFOUND) {
        return Err{"Value not found for key {}", key};
      }
      return Err{"Failed to read value for key {}. {}", key, rc.error()};
    }
    return Ok{
        std::string{static_cast<const char*>(value.mv_data), value.mv_size}};
  }

  result<void> NewGame() noexcept {
    if (const auto result = Reset(); !result) {
      return result.error();
    }
    const auto startResult = instance_->Start("");
    ready_ = true;
    return startResult;
  }

  result<void> Load(const std::string& saveFile) {
    if (const auto result = Reset(); !result) {
      return result.error();
    }
    if (!IsValidSaveName(saveFile)) {
      return Err{"Invalid argument (saveFile): {}", saveFile};
    }
    if (fs::exists(root_ / "saves" / saveFile)) {
      if (const auto result = instance_->Start(saveFile); !result) {
        return result.error();
      }
      ready_ = true;
      return Ok{};
    }
    return NewGame();
  }

  result<void> Save(const std::string& saveFile) const {
    if (!ready_) {
      return Err{"EDE not ready"};
    }
    if (!IsValidSaveName(saveFile)) {
      return Err{"Invalid argument (saveFile): {}", saveFile};
    }
    return instance_->Save(saveFile);
  }

  result<void> Reset() {
    instance_->Close();
    ready_ = false;
    return Ok{};
  }

  result<bool> Delete(const std::string& saveName) const {
    if (!IsValidSaveName(saveName)) {
      return Err{"Invalid argument (saveName): {}", saveName};
    }
    try {
      if (fs::exists(root_ / "saves" / saveName)) {
        fs::remove_all(root_ / "saves" / saveName);
        return Ok{true};
      }
      return Ok{false};
    } catch (std::exception& error) {
      return Err{"Failed to remove old save. {}", error.what()};
    }
  }

  _NODISCARD bool IsReady() const { return ready_; }
  void Shutdown() {
    instance_.reset();
    ready_ = false;
  }

 private:
  fs::path root_;
  std::unique_ptr<Instance> instance_;
  bool ready_{};
};
}  // namespace ExtraDataExtender
