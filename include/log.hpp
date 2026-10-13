#pragma once
#include <cstdio>
#include <mutex>
#include <string>

namespace elog {
    inline std::mutex& mtx() { static std::mutex m; return m; }
    inline void info(const char* fmt, ...) { va_list ap; va_start(ap, fmt); std::lock_guard<std::mutex> l(mtx()); vprintf(fmt, ap); printf("\n"); va_end(ap); }
    inline void ok(const char* fmt, ...)   { va_list ap; va_start(ap, fmt); std::lock_guard<std::mutex> l(mtx()); vprintf(fmt, ap); printf("\n"); va_end(ap); }
    inline void warn(const char* fmt, ...)  { va_list ap; va_start(ap, fmt); std::lock_guard<std::mutex> l(mtx()); vprintf(fmt, ap); printf("\n"); va_end(ap); }
    inline void err(const char* fmt, ...)   { va_list ap; va_start(ap, fmt); std::lock_guard<std::mutex> l(mtx()); vprintf(fmt, ap); printf("\n"); va_end(ap); }
}
