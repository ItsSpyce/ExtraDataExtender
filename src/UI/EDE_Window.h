#pragma once

#include "FUCK_API.h"
#include "REX/REX/Singleton.h"

namespace ExtraDataExtender::UI {
class EDEWindow : public FUCK::ITool, public REX::Singleton<EDEWindow> {
public:
  const char* Name() const override { return "Extra Data Extender"; }
  void Draw() override;
  void OnClose() override {}
  void OnOpen() override {}
};
}