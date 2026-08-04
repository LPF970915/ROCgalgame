#include <cstdlib>

extern "C" {

long roc_legacy_strtol(const char *, char **, int) __asm__("strtol");
unsigned long roc_legacy_strtoul(const char *, char **, int) __asm__("strtoul");
long long roc_legacy_strtoll(const char *, char **, int) __asm__("strtoll");
unsigned long long roc_legacy_strtoull(const char *, char **, int)
    __asm__("strtoull");

long __isoc23_strtol(const char *nptr, char **endptr, int base) {
  return roc_legacy_strtol(nptr, endptr, base);
}

unsigned long __isoc23_strtoul(const char *nptr, char **endptr, int base) {
  return roc_legacy_strtoul(nptr, endptr, base);
}

long long __isoc23_strtoll(const char *nptr, char **endptr, int base) {
  return roc_legacy_strtoll(nptr, endptr, base);
}

unsigned long long __isoc23_strtoull(const char *nptr, char **endptr, int base) {
  return roc_legacy_strtoull(nptr, endptr, base);
}

}  // extern "C"
