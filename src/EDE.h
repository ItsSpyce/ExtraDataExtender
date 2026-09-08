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

  void Initialize() {
    // TODO:
  }

 private:
  ExtraDataExtender::PluginInterfaceImpl* interface_ =
      new ExtraDataExtender::PluginInterfaceImpl();
};