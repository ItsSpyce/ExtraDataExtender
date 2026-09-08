#include "../include/EDE_API.h"

namespace {
using namespace ExtraDataExtender;
EDEPluginInterfaceV1* ede;

struct ExtraFavoriteColor {
  enum FavoriteColor { red, blue, green, yellow, orange, pink, black, white };
  FavoriteColor favoriteColor;
  int timesChanged = 0;

  using T = ExtraFavoriteColor;
  static constexpr auto bind = [](ExtraDataConstructor& ctor) {
    auto ctx = ctor.create_context<ExtraFavoriteColor>("FavoriteColor");
    ctx.bind("favoriteColor", &T::favoriteColor);
    ctx.bind("timesChanged", &T::timesChanged);
  };
};

static void OnMessage(SKSE::MessagingInterface::Message* msg) {
  if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
    ede = ExtraDataExtender::Query<EDEPluginInterfaceV1>();
  } else if (msg->type == SKSE::MessagingInterface::kPostLoadGame) {
    auto* player = RE::PlayerCharacter::GetSingleton();
  }
}
}  // namespace