# ExtraDataExtender

## Example

```cpp
#include <ExtraDataExtender.h>

struct MyExtraData : ExtraDataExtender::ExtraData {
  unsigned GetVersion() override { return 1; }
  const char* GetID() override { return "Example.MyExtraData"; }

  std::string foo;

  bool Read(ExtraDataExtender::SerializationStream* stream, unsigned version) override {
    // same as with SerializationInterface
  }

  bool Write(ExtraDataExtender::SerializationStream* stream) override {
    // also same
  }
}

void OnMessage(SKSE::MessagingInterface::Message* msg) {
  if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
    if (EDE_SUCCESS(ExtraDataExtender::RegisterDataType<MyExtraData>)) {
      SKSE::log::info("Registered my extra data");
    }
  }
}
```