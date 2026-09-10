#include "Hooks.h"

EDE_NAMESPACE::Hooks {
RE::NiAVObject* TESObjectREFR_Load3D::thunk(RE::TESObjectREFR* _this, bool a_arg1) {
  return func(_this, a_arg1);
}

RE::NiAVObject* Actor_Load3D::thunk(RE::Actor* _this, bool a_arg1) {
  return func(_this, a_arg1);
}

void TESObjectREFR_Release3DRelatedData::thunk(RE::TESObjectREFR* _this) {
  func(_this);
}
}