#pragma once

namespace ExtraDataExtender {
typedef uint64_t UniversalID;

template <typename For>
static UniversalID ParseUniversalID(For target);

template <>
static UniversalID ParseUniversalID(const RE::TESObjectREFR* target);

template <>
static UniversalID ParseUniversalID(const RE::InventoryEntryData* target);
}