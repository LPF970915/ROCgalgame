#include <cassert>
#include <cstring>
#include <limits>

extern "C" {
long __isoc23_strtol(const char *, char **, int);
unsigned long __isoc23_strtoul(const char *, char **, int);
long long __isoc23_strtoll(const char *, char **, int);
unsigned long long __isoc23_strtoull(const char *, char **, int);
}

namespace {
template <typename Value, typename Parse>
void ExpectParsed(const char *text, int base, Value expected, Parse parse) {
  char *end = nullptr;
  assert(parse(text, &end, base) == expected);
  assert(end != nullptr && *end == '\0');
}
}  // namespace

int main() {
  ExpectParsed("-123", 10, -123L, __isoc23_strtol);
  ExpectParsed("ff", 16, 255UL, __isoc23_strtoul);
  ExpectParsed("-9223372036854775807", 10,
               std::numeric_limits<long long>::min() + 1, __isoc23_strtoll);
  ExpectParsed("18446744073709551615", 10,
               std::numeric_limits<unsigned long long>::max(),
               __isoc23_strtoull);
  return 0;
}
