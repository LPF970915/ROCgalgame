#include "game_library.h"

const char *CoreKindName(CoreKind kind) {
  switch (kind) {
    case CoreKind::Ons: return "ons";
    case CoreKind::Krkr: return "krkr";
    case CoreKind::Tyrano: return "tyrano";
    default: return "unknown";
  }
}

const char *KrkrRuntimeName(KrkrRuntime runtime) {
  switch (runtime) {
    case KrkrRuntime::Sdl2: return "krkrsdl2";
    case KrkrRuntime::Krkr2: return "krkr2";
    case KrkrRuntime::Wine: return "wine";
    default: return "auto";
  }
}
