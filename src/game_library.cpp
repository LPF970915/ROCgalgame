#include "game_library.h"

const char *CoreKindName(CoreKind kind) {
  switch (kind) {
    case CoreKind::Ons: return "ons";
    case CoreKind::Krkr: return "krkr";
    case CoreKind::Tyrano: return "tyrano";
    default: return "unknown";
  }
}
