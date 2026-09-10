#include "EDE.h"
#include "ExtraDataExtender.h"

extern "C" __declspec(dllexport) void* QueryPluginInterface() {
  return &EDE::GetSingleton()->GetInterface();
}

static void OnMessage(SKSE::MessagingInterface::Message* msg) {
  if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
    //
  }
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse) {
  SKSE::Init(a_skse);
  SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
  EDE::Initialize();

  return true;
}
