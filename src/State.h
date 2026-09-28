#pragma once

#include "Database.h"
#include "FormIDManager.h"
#include "IDSolver.h"
#include "IDStore.h"

namespace ExtraDataExtender {
class State : public Singleton<State> {
  // should this be configurable?
  static inline const auto SAVE_PATH =
      fs::path{"Data/SKSE/Plugins/ExtraDataExtender"};

  enum class Phase : uint8_t {
    kActive,
    kPreloaded,
    kPreloadReady,
  };

 public:
  State() { db_ = std::make_unique<Database>(SAVE_PATH); }
  ~State() { db_.release(); }

  DONOTMOVEITMOVEIT(State);

  result<void> Load(const std::string& saveName) noexcept {
    ClearSession();
    // AI could never
    if (const auto loaded = db_->Load(saveName)) {
      if (const auto ledger = db_->Read("ede/identities/v1")) {
        if (const auto decoded = idSolver_.Decode(ledger.value())) {
          if (const auto forms = db_->Read("ede/forms/v1")) {
            if (const auto loadedForms = formIDs_.Decode(*forms)) {
              if (const auto remapped = idSolver_.RemapForms(
                      [this](const auto saved, auto& current) {
                        if (const auto resolved = formIDs_.Resolve(saved)) {
                          current = *resolved;
                          return true;
                        }
                        return false;
                      })) {
                phase_ = Phase::kPreloaded;
                return Ok{};
              } else {
                IGNORE(Reset());
                return remapped.error();
              }
            } else {
              IGNORE(Reset());
              return loadedForms.error();
            }
          } else {
            return forms.error();
          }
        } else {
          IGNORE(Reset());
          return decoded.error();
        }
      } else {
        if (ledger.error().code == MDB_NOTFOUND) {
          // this is fine, might be first load
          phase_ = Phase::kPreloaded;
          return Ok{};
        }
        return ledger.error();
      }
    } else {
      return Err{"Failed to load LMDB. {}", loaded.error()};
    }
  }

  result<void> NewGame() noexcept {
    ClearSession();
    if (const auto result = db_->NewGame()) {
      return Ok{};
    } else {
      return result.error();
    }
  }

  result<void> Save(const std::string& saveName) const noexcept {
    if (const auto result = db_->Save(saveName); !result) {
      return Err{"Failed to save to LMDB. {}", result.error()};
    }
    return Ok{};
  }

  result<void> Delete(const std::string& saveName) const {
    if (const auto result = db_->Delete(saveName); !result) {
      return Err{"Failed to delete LMDB save. {}", result.error()};
    }
    return Ok{};
  }

  result<void> PrepareSave() {
    if (const auto commitResult = CommitIDs()) {
      if (const auto flushResult = idStore_.Flush()) {
        return Ok{};
      } else {
        return flushResult.error();
      }
    } else {
      return commitResult.error();
    }
  }

  result<void> Reset() {
    ClearSession();
    if (db_) {
      if (const auto dbReset = db_->Reset(); !dbReset) {
        return dbReset.error();
      }
    }
    return Ok{};
  }

  void FinishLoad() {
    phase_ = Phase::kActive;
    formIDs_.Clear();
  }

  void FinishOutgoingRevert() { phase_ = Phase::kPreloadReady; }
  bool Preloaded() const { return phase_ != Phase::kActive; }
  bool PreloadReady() const { return phase_ == Phase::kPreloadReady; }
  result<void> CommitIDs() {
    if (!idSolver_.Dirty()) {
      return Ok{};
    }
    if (const auto forms = formIDs_.Encode(idSolver_.Forms())) {
      if (const auto result = db_->Write({{"ede/identities/v1", idSolver_.Encode()},
                                      {"ede/forms/v1", forms.value()}})) {
        idSolver_.MarkClean();
        return Ok{};
      } else {
        return Err{"Failed to commit remapped forms to LMDB. {}", result.error()};
      }
    } else {
      return Err{"Failed to encode form ID mappings. {}", forms.error()};
    }
  }

  // probably won't ever be called
  void Shutdown() {
    ClearSession();
    if (db_) {
      db_->Shutdown();
    }
  }

  Database& GetDb() const { return *db_; }
  IDSolver& GetIDSolver() { return idSolver_; }
  IDStore& GetIDStore() { return idStore_; }

 private:
  std::unique_ptr<Database> db_;
  Phase phase_{Phase::kActive};
  IDSolver idSolver_{};
  IDStore idStore_{};
  FormIDManager formIDs_{};

  void ClearSession() {
    IGNORE(idStore_.Reset());
    formIDs_.Clear();
    idSolver_.Clear();
    phase_ = Phase::kActive;
  }
};
}  // namespace ExtraDataExtender