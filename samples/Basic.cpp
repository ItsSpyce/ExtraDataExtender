#include "../include/ExtraDataExtender.h"

struct ExampleData {
  std::string foo;
  std::string bar;
};

template <>
struct ExtraDataExtender::Codex<ExampleData> {
  using T = ExampleData;
  constexpr auto name = typeid(ExampleData).name;
  constexpr auto value = shape(
    field | "foo" = &T::foo,
    field | "bar" = &T::bar
  );
};
