#include <array>

#include "display_mode.h"

static std::array<std::function<DisplayMode *()>,
                  static_cast<int>(DisplayMode::Type::kNum)>
    creators;

bool DisplayMode::registCreator(Type type,
                                std::function<DisplayMode *()> &&fn) {
  creators[static_cast<int>(type)] = std::move(fn);
  return true;
}

DisplayMode *DisplayMode::create(Type type) {
  return creators[static_cast<int>(type)]();
}
