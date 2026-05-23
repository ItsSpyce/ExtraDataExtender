#include "ExtraDataExtender.h"

namespace ExtraDataExtender {
namespace {
void VisitExtraData(RE::InventoryEntryData* a_data) {
  
}
}

EDEExtraData* Add(RE::ExtraDataList* _this, EDEExtraData* toAdd) {
  auto uid = _this->GetByType<RE::ExtraUniqueID>();
}

bool Remove(RE::ExtraDataList* _this, EDEExtraData* toRemove) {
  
}

bool HasType(RE::ExtraDataList* _this, uint32_t type) {
  
}

EDEExtraData* GetByType(RE::ExtraDataList* _this, uint32_t type) {
  
}
}