#pragma once

#include "RE/Additional.h"

EDE_NAMESPACE::Utils {
  inline fn CreateExtraDataList(RE::InventoryEntryData * data) -> RE::ExtraDataList* {
    if (!data) return nullptr;
    if (!data->extraLists) {
      data->extraLists = new RE::BSSimpleList<RE::ExtraDataList*>();
    }
    if (!data->extraLists->empty()) {
      return data->extraLists
          ->front();  // TODO: is this just returning the first item?
    }
    auto* alloc = RE::MemoryManager::GetSingleton()->Allocate(
        sizeof(RE::ExtraDataList), 0, false);
    auto* result = RE::Additional::ExtraDataList_Ctor(alloc);
    data->AddExtraList(result);
    return result;
  }
}