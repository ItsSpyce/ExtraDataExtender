#pragma once

#define EDE_BUILD_HOST 1

#include <RE/Skyrim.h>
#include <REX/REX.h>
#include <SKSE/SKSE.h>
#include <SimpleIni.h>

#include "STL.h"

namespace logger = SKSE::log;
using namespace std::literals;

using NativeUID = decltype(RE::ExtraUniqueID::uniqueID);
using uid_t = uint64_t;

namespace fs = std::filesystem;

#define NOCOPY(_T) _T(const _T&) = delete
#define NOCOPYASS(_T) _T& operator=(const _T&) = delete
#define NOMOVE(_T) _T(_T&&) = delete
#define NOMOVEASS(_T) _T&& operator=(_T&&) = delete

#define DONOTMOVEITMOVEIT(_T)        \
  _T(const _T&) = delete;            \
  _T(_T&&) = delete;                 \
  _T& operator=(const _T&) = delete; \
  _T&& operator=(_T&&) = delete

#define _SAFE noexcept