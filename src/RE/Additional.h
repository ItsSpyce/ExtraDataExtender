#pragma once

namespace RE::Additional {
inline ExtraDataList* ExtraDataList_Ctor(void* _this) {
  using Func = decltype(&ExtraDataList_Ctor);
  static const REL::Relocation<Func> func{RELOCATION_ID(11437, 11583)};
  return func(_this);
}
}