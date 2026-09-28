#pragma once

#include "UniqueID.h"

namespace ExtraDataExtender::Hooks {
template <class T>
  requires std::is_base_of_v<RE::TESObjectREFR, T>
struct Lifecycle {
  static RE::NiAVObject* Load(T* ref, bool background) {
    const auto* node = LoadFunc(ref, background);
    if (node) {
      UniqueID::BeginTrackingRefr(ref->GetFormID());
    } else {
      UniqueID::StopTrackingRefr(ref->GetFormID());
    }
    return node;
  }

  static void Unload(T* ref) {
    UnloadFunc(ref);
    UniqueID::StopTrackingRefr(ref->GetFormID());
  }

  static RE::ObjectRefHandle Remove(T* owner, RE::TESBoundObject* obj,
                                    int32_t count,
                                    RE::ITEM_REMOVE_REASON reason,
                                    RE::ExtraDataList* list,
                                    RE::TESObjectREFR* destination,
                                    const RE::NiPoint3* position,
                                    const RE::NiPoint3* rotation) {
    const auto* anchor = list ? list->GetByType<RE::ExtraUniqueID>() : nullptr;
    const auto nativeUID = anchor ? anchor->uniqueID : NativeUID{};
    auto result = RemoveFunc(owner, obj, count, reason, list, destination,
                             position, rotation);
    if (!destination && nativeUID != NULL && count > 0) {
      if (const auto world = result.get()) {
        UniqueID::OnDropped(owner->GetFormID(), obj ? obj->GetFormID() : NULL,
                            nativeUID, world.get());
      }
    }
    return result;
  }

  static RE::ObjectRefHandle Drop(T* owner, const RE::TESBoundObject* obj,
                                  RE::ExtraDataList* list, int32_t count,
                                  const RE::NiPoint3* position,
                                  const RE::NiPoint3* rotation) {
    const auto* anchor = list ? list->GetByType<RE::ExtraUniqueID>() : nullptr;
    const auto nativeUID = anchor ? anchor->uniqueID : NativeUID{};
    auto result = DropFunc(owner, obj, list, count, position, rotation);
    if (nativeUID != NULL && count > 0) {
      if (const auto world = result.get()) {
        UniqueID::OnDropped(owner->GetFormID(), obj ? obj->GetFormID() : NULL,
                            nativeUID, world.get());
      }
    }
    return result;
  }

  static void Pickup(T* owner, RE::TESObjectREFR* world, int32_t count,
                     bool arg3, bool playSound) {
    UniqueID::PickupCtx ctx{owner, world, count};
    PickupFunc(owner, world, count, arg3, playSound);
  }

  static void Install() {
    REL::Relocation vtbl{T::VTABLE[0]};
    LoadFunc = vtbl.write_vfunc(0x6A, Load);
    UnloadFunc = vtbl.write_vfunc(0x6B, Unload);
    RemoveFunc = vtbl.write_vfunc(0x56, Remove);
    if constexpr (std::is_base_of_v<RE::Actor, T>) {
      DropFunc = vtbl.write_vfunc(0xCB, Drop);
      PickupFunc = vtbl.write_vfunc(0xCC, Pickup);
    }
  }

  static inline REL::Relocation<decltype(Load)> LoadFunc;
  static inline REL::Relocation<decltype(Unload)> UnloadFunc;
  static inline REL::Relocation<decltype(Remove)> RemoveFunc;
  static inline REL::Relocation<decltype(Drop)> DropFunc;
  static inline REL::Relocation<decltype(Pickup)> PickupFunc;
};

inline void Install() {
  Lifecycle<RE::TESObjectREFR>::Install();
  Lifecycle<RE::Actor>::Install();
  Lifecycle<RE::Character>::Install();
  Lifecycle<RE::PlayerCharacter>::Install();
  Lifecycle<RE::Projectile>::Install();
  Lifecycle<RE::MissileProjectile>::Install();
  Lifecycle<RE::ArrowProjectile>::Install();
  Lifecycle<RE::GrenadeProjectile>::Install();
  Lifecycle<RE::BeamProjectile>::Install();
  Lifecycle<RE::FlameProjectile>::Install();
  Lifecycle<RE::ConeProjectile>::Install();
  Lifecycle<RE::BarrierProjectile>::Install();
  Lifecycle<RE::Explosion>::Install();
  Lifecycle<RE::Hazard>::Install();
}
}  // namespace ExtraDataExtender::Hooks