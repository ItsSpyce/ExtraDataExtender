#pragma once

#include "PluginInterfaceImpl.h"
#include "REX/REX/Singleton.h"

class EDE : public REX::Singleton<EDE> {
 public:
  EDE() = default;
  ~EDE() = default;

  _NODISCARD ExtraDataExtender::PluginInterfaceImpl& GetInterface() const {
    return *interface_;
  }

  static void Initialize() {
    SKSE::GetSerializationInterface()->SetLoadCallback(OnLoad);
    SKSE::GetSerializationInterface()->SetSaveCallback(OnSave);
  }

  static void OnLoad(SKSE::SerializationInterface* save) {}

  static void OnSave(SKSE::SerializationInterface* save) {}

 private:
  auto* interface_ = new ExtraDataExtender::PluginInterfaceImpl();
};