#include "ExtraDataExtender.h"
#include "Save.h"

#include "UniqueID.h"

using namespace ExtraDataExtender;

static void OnMessage(SKSE::MessagingInterface::Message* msg) {
  switch (msg->type) {
    case SKSE::MessagingInterface::kDataLoaded: {
      logger::info("Initializing");
      break;
    }
    case SKSE::MessagingInterface::kPreLoadGame: {
      logger::info("Loading");
      std::string saveFile{static_cast<char*>(msg->data), msg->dataLen - 4};
      logger::info("Save file: {}", saveFile);
      if (const auto result = Save::GetSingleton()->Read(saveFile); !result) {
        logger::error("Failed to load EDE save: {}", result.error());
      }
      break;
    }
    case SKSE::MessagingInterface::kSaveGame: {
      logger::info("Saving");
      std::string saveFile{static_cast<char*>(msg->data), msg->dataLen};
      logger::info("Save file: {}", saveFile);
      if (const auto result = Save::GetSingleton()->Write(saveFile); !result) {
        logger::error("Failed to write EDE save: {}", result.error());
      }
      break;
    }
    case SKSE::MessagingInterface::kDeleteGame: {
      logger::info("Deleting");
      std::string saveFile{static_cast<char*>(msg->data), msg->dataLen};
      logger::info("Save file: {}", saveFile);
      Save::GetSingleton()->Delete(saveFile).assert_ok();
      break;
    }
    case SKSE::MessagingInterface::kNewGame: {
      logger::info("New game");
      if (const auto result = Save::GetSingleton()->Read(""); !result) {
        logger::error("Failed to initialize EDE for new game: {}",
                      result.error());
      }
    }
    default:
      break;
  }
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* skse) {
  SKSE::Init(skse);
  spdlog::flush_on(spdlog::level::info);  // TODO
  SKSE::GetMessagingInterface()->RegisterListener(OnMessage);

  return true;
}