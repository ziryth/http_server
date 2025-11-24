#include "base_log.h"

void log_log(const char *level, const char *file, i32 line_number, const char *format, ...) {
    printf("[%s] %s:%d\t", level, file, line_number);

    va_list args;
    va_start(args, format);

    vprintf(format, args);

    va_end(args);
}
