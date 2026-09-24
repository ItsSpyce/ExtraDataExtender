#pragma once

#include "LMDB/lmdb++.h"

namespace LMDB {
class Connection {
  using epoch_t = uint64_t;
  static constexpr char SCHEMA_VERSION[] = {1, 0, 0, 0};
  static constexpr size_t INITIAL_MAP_SIZE = 64ULL * 1024;           // 64kb
  static constexpr size_t MAX_MAP_SIZE = 1ULL * 1024 * 1024 * 1024;  // 1gb

 public:
  using Mutation = std::pair<std::string, std::optional<std::string>>;

  DONOTMOVEITMOVEIT(Connection);
  Connection() : db_(lmdb::env::create()) {
    db_.set_mapsize(INITIAL_MAP_SIZE);
    db_.set_max_dbs(2);
  }
  ~Connection() { db_.close(); }

  result<bool> Write(std::vector<Mutation> changes) const noexcept {
    auto txn = lmdb::txn::begin(db_);
    auto dbi = lmdb::dbi::open(txn, nullptr);
    for (const auto& [key, value] : changes) {
      try {
        if (value) {
          if (!dbi.put(txn, key, *value)) {
            txn.abort();
            return Err{"Failed to update row with key {}", key};
          }
        } else {
          if (!dbi.del(txn, key)) {
            txn.abort();
            return Err{"Failed to delete row with key {}", key};
          }
        }
      } catch (lmdb::error& err) {
        txn.abort();
        return Err{"An error occured while mutating LMDB: {}", err.what()};
      }
    }
    try {
      txn.commit();
      return Ok{txn.handle() == nullptr};
    } catch (lmdb::error& err) {
      txn.abort();
      return Err{"An error occured while commiting to LMDB: {}", err.what()};
    }
  }

  result<std::string> Read(std::string key) const {
    try {
      auto txn = lmdb::txn::begin(db_);
      const auto dbi = lmdb::dbi::open(txn, nullptr);
      auto cursor = lmdb::cursor::open(txn, dbi);
      std::string val;
      if (cursor.find(key, MDB_FIRST)) {
        cursor.get(key, val, MDB_FIRST);
      }
      cursor.close();
      txn.abort();
      if (!val.empty()) {
        return Ok{val};
      }
      return Err{"Key not found: {}", key};
    } catch (lmdb::error& err) {
      return Err{"Failed to read from LMDB: {}", err.what()};
    }
  }

  result<bool> Load(const fs::path& path) {
    try {
      if (db_.handle()) {
        db_.close();
      }
      db_ = lmdb::env::create();
      db_.set_mapsize(INITIAL_MAP_SIZE);
      db_.set_max_dbs(2);
      db_.open(path.string().c_str());
      return Ok{true};
    } catch (lmdb::error& err) {
      return Err{"Failed to start LMDB: {}", err.what()};
    }
  }

  result<bool> Save() {
    try {
      db_.sync();
      return Ok{true};
    } catch (lmdb::error& err) {
      return Err{"Failed to flush data to disk: {}", err.what()};
    }
  }

 private:
  lmdb::env db_;
};
}  // namespace LMDB