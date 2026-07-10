#include "EDE.h"
#include "FUCK_API.h"
#include "UI/EDE_Window.h"

extern "C" __declspec(dllexport) void* QueryPluginInterface() {
  return &EDE::GetSingleton()->GetInterface();
}

static void OnMessage(SKSE::MessagingInterface::Message* msg) {
  if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
    if (FUCK::Connect("ExtraDataExtender")) {
      FUCK::RegisterTool(ExtraDataExtender::UI::EDEWindow::GetSingleton());
    }
  }
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse) {
  SKSE::Init(a_skse);
  SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
  EDE::GetSingleton()->Initialize();

  return true;
}
