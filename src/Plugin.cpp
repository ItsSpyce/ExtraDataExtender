#include "ExtraDataExtender.h"
#include "Hooks.h"
#include "State.h"

using namespace ExtraDataExtender;

static result<std::string> GetSaveName(
    const SKSE::MessagingInterface::Message& message) {
  if (!message.data || message.dataLen == NULL) return Err{"Missing save name"};
  std::string name{static_cast<const char*>(message.data), message.dataLen};
  trim_end(name);
  if (name.ends_with(".ess")) {
    name.resize(name.size() - 4);
  }
  if (name.empty()) {
    return Err{"Empty save name"};
  }
  return Ok{std::move(name)};
}

static void OnMessage(SKSE::MessagingInterface::Message* msg) {
  switch (msg->type) {
    case SKSE::MessagingInterface::kDataLoaded: {
      logger::info("Initializing");
      IDSolver::RegisterEvents();
      break;
    }
    case SKSE::MessagingInterface::kPreLoadGame: {
      logger::info("Preloading");
      if (const auto saveName = GetSaveName(*msg)) {
        logger::info("Using save name {}", *saveName);
        IDSolver::Reset();
        logger::info("Setting up LMDB");
        if (const auto loaded = State::GetSingleton()->Load(saveName.value())) {
          logger::info("Preload complete");
        } else {
          logger::error("Failed to load save: {}", loaded.error());
        }
      } else {
        logger::error("Failed to read save name: {}", saveName.error());
      }
      break;
    }
    case SKSE::MessagingInterface::kPostLoadGame: {
      logger::info("Loading");
      if (msg->data) {
        State::GetSingleton()->FinishLoad();
        IDSolver::Resume();
        logger::info("Finished loading game");
      } else {
        IDSolver::Reset();
        if (const auto result = State::GetSingleton()->Reset()) {
          logger::info("Finished loading game");
        } else {
          logger::error("Failed to reset state: {}", result.error());
        }
      }
      break;
    }
    case SKSE::MessagingInterface::kSaveGame: {
      logger::info("Saving");
      struct FinishOnLeave {
        ~FinishOnLeave() { IDSolver::FinishSave(); }
      } finish;
      if (const auto saveName = GetSaveName(*msg)) {
        if (const auto identity = IDSolver::PrepareSave()) {
          if (const auto prepared = State::GetSingleton()->PrepareSave()) {
            if (const auto saved =
                    State::GetSingleton()->Save(saveName.value())) {
              logger::info("Save complete");
            } else {
              logger::error("Failed to save: {}", saved.error());
            }
          } else {
            logger::error("Failed to prepare state save: {}", prepared.error());
          }
        } else {
          logger::error("Failed to prepare global save: {}", identity.error());
        }
      } else {
        logger::error("Failed to read save name: {}", saveName.error());
      }
      break;
    }
    case SKSE::MessagingInterface::kDeleteGame: {
      break;
    }
    case SKSE::MessagingInterface::kNewGame: {
      logger::info("New game");
      IDSolver::Reset();
      if (const auto opened = State::GetSingleton()->NewGame()) {
        IDSolver::Resume();
        logger::info("New game initialized");
      } else {
        logger::error("Failed to initialize new game: {}", opened.error());
      }
      break;
    }
    default:
      break;
  }
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* skse) {
  SKSE::Init(skse);
  spdlog::flush_on(spdlog::level::info);  // TODO

  Hooks::Install();
  return SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
}