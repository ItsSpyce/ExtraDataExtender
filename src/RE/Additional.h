#pragma once

namespace RE::Additional {
inline ExtraDataList* ConstructExtraDataList(void* _this) {
  using Func = decltype(&ConstructExtraDataList);
  static const REL::Relocation<Func> func{RELOCATION_ID(11437, 11583)};
  auto* constructed = func(_this);
}
}