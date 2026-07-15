#include "platform.h"

PlatformLayout ResolveLayout(const AppConfig &config) {
  PlatformLayout layout;
  layout.screen_w = config.screen_w;
  layout.screen_h = config.screen_h;
  return layout;
}
