#pragma once

#include "EDE_API.h"

EDE_NAMESPACE {
class PluginInterfaceImpl : public EDEPluginInterfaceV1 {
 public:
  enum ExtraDataKind : uint8_t {
    kString,
    kByte,
    kUShort,
    kUInt,
    kULong,
    kSByte,
    kShort,
    kInt,
    kLong,
    kFloat,
    kDouble,
    kBool,

    kMax,
  };
  struct ExtraDataRecord {
    ExtraDataKind kind;
    std::any value;
  };
  class EDESaveDataImpl : public EDESaveData {
   public:
    bool Write(const std::string& field, const std::any& value) override;
    std::any Read(const std::string& field) override;
    bool Contains(const std::string& field) override;

   private:
    std::unordered_map<std::string, ExtraDataRecord> records_;
  };

  PluginInterfaceImpl() = default;
  ~PluginInterfaceImpl() override = default;

  EDEExtraData* Add(RE::ExtraDataList* _this, EDEExtraData* toAdd) override;
  bool HasType(RE::ExtraDataList* _this, EDE_TYPE type) override;
  EDEExtraData* GetByType(RE::ExtraDataList* _this, EDE_TYPE type) override;
  bool Remove(RE::ExtraDataList* _this, EDEExtraData* toRemove) override;
  bool RemoveByType(RE::ExtraDataList* _this, EDE_TYPE type) override;
};

}  // namespace ExtraDataExtender