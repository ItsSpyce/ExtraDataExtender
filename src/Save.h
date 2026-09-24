#pragma once

#include "Helpers.h"
#include "LMDB/Connection.h"

namespace ExtraDataExtender {
namespace fs = std::filesystem;

/**
 * Pretty simple stuff. When a new game occurs or there's no previous
 * installation, EDE will create a .temp directory. That will contain the
 * current work until Write is called which will copy the .temp contents into a
 * folder named after the save. Loading an existing save is simply getting the
 * folder and copying those contents into the .temp folder. Ez pz, yeah?
 *
 * NO. IT'S NOT. I've been bashing my head for days trying to wrap my head
 * around the concept despite designing it while writing it.
 */
class Save : public Singleton<Save> {
  static inline const auto SavePath =
      fs::path{"Data/SKSE/Plugins/ExtraDataExtender/saves"};

 public:
  Save() { db_ = std::make_unique<LMDB::Connection>(); }

  ~Save() { db_.release(); }

  result<bool> Read(const std::string& saveName) const noexcept {
    const auto tempPath = SavePath;
    if (!fs::exists(tempPath) && !fs::create_directories(tempPath)) {
      return Err{"Failed to create temp directory"};
    }
    // empty = new game
    if (!saveName.empty()) {
      const auto savePath = SavePath / saveName;  // should remove the .ess?

      if (fs::exists(savePath)) {
        // copy previous save
        for (fs::directory_iterator it{tempPath}; auto& entry : it) {
          if (!entry.is_regular_file()) continue;
          logger::info("Copying previous save file {}", entry.path().string());
          if (!fs::copy_file(entry.path(), savePath / entry.path().filename(),
                             fs::copy_options::overwrite_existing)) {
            return Err{"Failed to copy previous save file: {}",
                       entry.path().string()};
          }
        }
      } else {
        // not found, either fresh install on an existing modlist or a failed
        // save prior
        logger::warn("Previous save data not found, looking for {}",
                     savePath.string());
      }
    }
    return db_->Load(tempPath);
  }

  // returns nothing tbh
  result<bool> Write(const std::string& saveName) const noexcept {
    // move from temp to 'saveName'
    const auto savePath = SavePath / saveName;
    const auto tempPath = SavePath;
    if (!fs::exists(tempPath)) {
      // shouldn't be possible, return an error
      return Err{"Save was not hooked into, skipping"};
    }
    if (!db_->Save()) {
      return Err{"Failed to save to LMDB"};
    }
    if (fs::exists(savePath)) {
      // previous save found under this name. Really not good
      return Err{"Duplicate save entry found"};
    }
    fs::create_directory(savePath);
    for (fs::directory_iterator it{savePath}; auto& entry : it) {
      if (!entry.is_regular_file()) continue;
      logger::info("Copying previous save file {}", entry.path().string());
      fs::copy_file(entry.path(), tempPath / entry.path().filename(),
                    fs::copy_options::overwrite_existing);
    }
    return Ok{true};
  }

  result<bool> Delete(const std::string& saveName) {
    const auto savePath = SavePath / saveName;
    if (!fs::exists(savePath)) {
      return Ok{false};
    }
    fs::remove_all(savePath);
    return Ok{true};
  }

  LMDB::Connection& GetDb() const {
    return *db_;
  }

 private:
  std::unique_ptr<LMDB::Connection> db_;
};
}  // namespace ExtraDataExtender