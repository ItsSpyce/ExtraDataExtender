#pragma once

#include "ExtraDataExtender.h"

namespace ExtraDataExtender::Errors {
inline Err NullArg(const std::string& fieldName) {
  return Err{EDE_InvalidArgument, "Expected non-null argument ({})", fieldName};
}

inline Err InvalidArg(const std::string& fieldName,
                         const std::string& what) {
  return Err{EDE_InvalidArgument, "Invalid argument ({}). {}", fieldName, what};
}
}  // namespace ExtraDataExtender::Errors