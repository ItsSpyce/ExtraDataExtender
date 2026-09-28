#pragma once

#include <emhash/hash_table5.hpp>
#include <emhash/hash_table8.hpp>

#include "StringReader.h"
#include "StringWriter.h"

namespace ExtraDataExtender {
class FormIDManager {
  struct Record {
    RE::FormID saved = NULL;
    bool isDynamic = false;
    RE::FormID localFormID = NULL;
    uint16_t plugin = UINT16_MAX;
  };

 public:
  result<std::string> Encode(const std::set<RE::FormID>& forms) {
    auto* data = RE::TESDataHandler::GetSingleton();
    emhash8::HashMap<std::string, uint16_t> pluginStringTable;
    std::vector<std::string> plugins;
    std::vector<Record> records;
    records.reserve(forms.size());
    for (const auto form : forms) {
      if (form == NULL) return Err{"Received empty form ID"};
      if (form >= 0xFF000000) {
        records.push_back({form, true});
        continue;
      }
      const auto isLight = (form & 0xFF000000) == 0xFE000000;
      const auto* file =
          isLight
              ? data->LookupLoadedLightModByIndex(
                    static_cast<uint16_t>(form >> 12 & 0xFFF))
              : data->LookupLoadedModByIndex(static_cast<uint8_t>(form >> 24));
      if (!file) {
        return Err{"Plugin lookup failed for form {:08X}", form};
      }
      const std::string plugin{file->GetFilename()};
      const auto [entry, inserted] = pluginStringTable.try_emplace(
          plugin, static_cast<uint16_t>(plugins.size()));
      if (inserted) {
        plugins.push_back(plugin);
      }
      records.push_back(
          {.saved = form,
           .isDynamic = false,
           .localFormID = isLight ? form & 0xFFF : form & 0xFFFFFF,
           .plugin = entry->second});
    }
    StringWriter bytes;
    bytes.Write("EDEF");
    // TODO: make an actual string table implementation
    // schema version
    bytes.Write(1, 1);
    // plugin size
    bytes.Write(plugins.size(), 2);
    // records count
    bytes.Write(records.size(), 8);
    for (const auto& plugin : plugins) {
      bytes.Write(plugin.size(), 2);
      bytes.Write(plugin);
    }
    for (const auto& record : records) {
      bytes.Write(record.saved, 4);
      bytes.Write(record.isDynamic, 1);
      bytes.Write(record.localFormID, 4);
      bytes.Write(record.plugin, 2);
    }
    return Ok{bytes.ToString()};
  }

  result<bool> Decode(std::string_view bytes) {
    StringReader reader{bytes};
    if (reader.AtEnd()) return Err{"Empty form table string"};
    if (reader.ReadString(4) != "EDEF") return Err{"Invalid form header"};
    const auto schema = reader.ReadByte();
    if (schema == 1) {
      // schema 1.0
      const auto pluginCount = reader.ReadUShort();
      const auto recordsCount = reader.ReadULong();
      std::vector<std::string> plugins;
      plugins.reserve(pluginCount);

      for (uint64_t i = 0; i < pluginCount; ++i) {
        const auto length = reader.ReadUShort();
        if (length == 0 || length > reader.Remaining()) {
          return Err{"Invalid record length"};
        }
        auto plugin = reader.ReadString(length);
        plugins.push_back(std::move(plugin));
      }
      emhash5::HashMap<RE::FormID, std::optional<RE::FormID>> replacement;
      auto* data = RE::TESDataHandler::GetSingleton();
      for (uint64_t i = 0; i < recordsCount; ++i) {
        const auto saved = reader.ReadUInt();
        const auto kind = reader.ReadByte();
        const auto local = reader.ReadUInt();
        const auto plugin = reader.ReadUShort();
        if (!saved || kind > 1) {
          return Err{"Invalid form record: invalid kind"};
        }
        if (plugin >= pluginCount) {
          return Err{"Out of range plugin. Max {}, got {}", pluginCount,
                     plugin};
        }
        const auto savedLight = (saved & 0xFF000000) == 0xFE000000;
        const auto expectedLocal =
            savedLight ? saved & 0xFFF : saved & 0xFFFFFF;
        if ((kind == 1 &&
             (saved < 0xFF000000 || local || plugin != UINT32_MAX)) ||
            (kind == 0 && (saved >= 0xFF000000 || plugin >= pluginCount ||
                           local != expectedLocal)) ||
            replacement.contains(saved)) {
          return Err{"Invalid form table record: bad data"};
        }
        if (kind == 1) {
          // dynamics don't change, insert and continue
          replacement.emplace(saved, saved);
        } else if (savedLight) {
          replacement.emplace(saved, std::nullopt);
        } else {
          const auto& pluginName = plugins[plugin];
          if (const auto* form = data->LookupForm(local, pluginName)) {
            replacement.emplace(saved, form->GetFormID());
          }
        }
      }
      if (!reader.AtEnd()) {
        return Err{"Encountered trailing save data: {}", reader.ReadToEnd()};
      }
      mappings_ = replacement;  // emhash already moves
      return Ok{true};
    }

    return Err{"Unsupported schema version {}", schema};
  }

  bool Contains(RE::FormID saved) const { return mappings_.contains(saved); }

  std::optional<RE::FormID> Resolve(RE::FormID saved) {
    const auto it = mappings_.find(saved);
    return it == mappings_.end() ? std::nullopt : it->second;
  }

  void Clear() { mappings_.clear(); }

 private:
  emhash5::HashMap<RE::FormID, std::optional<RE::FormID>> mappings_;
};
}  // namespace ExtraDataExtender