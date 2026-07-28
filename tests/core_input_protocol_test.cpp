#include "core_input_protocol.h"

#include <cassert>
#include <cmath>
#include <iostream>

int main() {
  assert(CoreAxisStateNeedsSend(0.0f, 0.0f, 0.0f, 0.0f, false, 0));
  assert(!CoreAxisStateNeedsSend(0.0f, 0.0f, 0.0f, 0.0f, true, 149));
  assert(CoreAxisStateNeedsSend(0.0f, 0.0f, 0.0f, 0.0f, true, 150));
  assert(CoreAxisStateNeedsSend(0.5f, 0.0f, 0.0f, 0.0f, true, 1));
  assert(CoreAxisStateNeedsSend(0.0f, 0.0f, 0.5f, 0.0f, true, 1));
  assert(!CoreAxisStateNeedsSend(0.0005f, 0.0005f, 0.0f, 0.0f, true, 1));
  assert(EncodeCoreAxisState(0.5f, -0.25f, 42) ==
         "A 0.500000 -0.250000 42\n");
  assert(EncodeCoreAxisState(2.0f, -2.0f, 43) ==
         "A 1.000000 -1.000000 43\n");

  const auto near = [](float actual, float expected) {
    return std::abs(actual - expected) < 0.01f;
  };
  for (float frame : {0.0167f, 0.033f, 0.1f, 0.7f}) {
    const CorePointerDelta delta =
        IntegrateCorePointer(1.0f, 0.0f, frame, 1080.0f, 1.0f);
    assert(near(delta.x, 1080.0f * frame));
    assert(near(delta.y, 0.0f));
  }
  const CorePointerDelta half =
      IntegrateCorePointer(0.5f, 0.0f, 1.0f, 1080.0f, 1.0f);
  assert(near(half.x, 540.0f));
  const CorePointerDelta diagonal =
      IntegrateCorePointer(1.0f, 1.0f, 1.0f, 1080.0f, 1.0f);
  assert(near(std::sqrt(diagonal.x * diagonal.x + diagonal.y * diagonal.y),
              1080.0f));
  const CorePointerDelta suspended =
      IntegrateCorePointer(1.0f, 0.0f, 0.0f, 1080.0f, 1.0f);
  assert(near(suspended.x, 0.0f));
  std::cout << "core input protocol tests passed\n";
}
