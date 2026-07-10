#pragma once
#include "STL.h"

namespace ExtraDataExtender::Hooks {
struct TESObjectREFR_Load3D {
  static RE::NiAVObject* thunk(RE::TESObjectREFR* _this, bool a_arg1);
  static inline REL::Relocation<decltype(&thunk)> func;
  static constexpr size_t idx{0x6A};
};

struct TESObjectREFR_Release3DRelatedData {
  static void thunk(RE::TESObjectREFR* _this);
  static inline REL::Relocation<decltype(&thunk)> func;
  static constexpr size_t idx{0x6B};
};

struct Actor_Load3D {
  static RE::NiAVObject* thunk(RE::Actor* _this, bool a_arg1);
  static inline REL::Relocation<decltype(&thunk)> func;
  static constexpr size_t idx{0x6A};
};

inline void Register() {
  stl::write_vfunc<RE::TESObjectREFR, TESObjectREFR_Load3D>();
  stl::write_vfunc<RE::TESObjectREFR, TESObjectREFR_Release3DRelatedData>();
  stl::write_vfunc<RE::Actor, Actor_Load3D>();
}
}