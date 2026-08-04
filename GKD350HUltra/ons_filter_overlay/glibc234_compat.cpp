#include <cstdarg>
#include <cstdio>
#include <cstdlib>

extern "C" long roc_legacy_strtol(const char *, char **, int) __asm__("strtol");
extern "C" int roc_legacy_vfscanf(FILE *, const char *, va_list)
    __asm__("__isoc99_vfscanf");
extern "C" int roc_legacy_vsscanf(const char *, const char *, va_list)
    __asm__("__isoc99_vsscanf");

extern "C" long __isoc23_strtol(const char *text, char **end, int base) {
    return roc_legacy_strtol(text, end, base);
}

extern "C" int __isoc23_fscanf(FILE *stream, const char *format, ...) {
    va_list args;
    va_start(args, format);
    const int result = roc_legacy_vfscanf(stream, format, args);
    va_end(args);
    return result;
}

extern "C" int __isoc23_sscanf(const char *text, const char *format, ...) {
    va_list args;
    va_start(args, format);
    const int result = roc_legacy_vsscanf(text, format, args);
    va_end(args);
    return result;
}
