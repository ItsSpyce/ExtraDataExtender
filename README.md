# ExtraDataExtender

> A Skyrim mod for adding save-scoped data to references or inventory items.

## Usage

0. Install the mod (obviously, but maybe I should state that in plain text)
1. Copy `include/EDE_API.h` into your project
2. In your plugin startup (preferrably post `kDataLoaded`), call `ExtraDataExtender::Query()` to ensure EDE exists

```cpp
#include <EDE_API.h>

namespace {
struct MyExtraData {
  std::string foo;
  int answerToLife;
};

// more shit
ExtraDataExtender::EDEPluginInterfaceV1* ede;

static void OnMessage(SKSE::MessagingInterface::Message* msg) {
  if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
    ede = ExtraDataExtender::Query();
    ede->RegisterType<MyExtraData>();
  } else if (msg->type == SKSE::MessagingInterface::kNewGame) {
    ede->AddExtraData(RE::PlayerCharacter::GetSingleton(), MyExtraData{
      .foo = "bar",
      .answerToLife = 42,
    });
  }
}
}
```

## Extra data requirements

1. The only valid data types are `scalar`, `array`, or `struct`. Any complex types that require inference are not supported.
2. Each field is to be verbosely defined in the struct's `bind` method.
3. An extra data kind MUST be trivial to construct. This is by design to permit reflected bindings. 

## Functions

### V1

#### `RegisterType<T>`

Registers an extra data type to be persisted.