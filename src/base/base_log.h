#ifndef BASE_LOG_H
#define BASE_LOG_H

#include "base_core.h"
#include <stdarg.h>
#include <stdio.h>

#define log_trace(...) log_log("\x1b[94mTRACE\x1b[0m", __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_log("\x1b[36mDEBUG\x1b[0m", __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...) log_log("\x1b[32mINFO\x1b[0m", __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...) log_log("\x1b[33mWARN\x1b[0m", __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log_log("\x1b[31mERROR\x1b[0m", __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log_log("\x1b[35mFATAL\x1b[0m", __FILE__, __LINE__, __VA_ARGS__)

void log_log(const char *level, const char *file, i32 line_number, const char *format, ...);

#endif // BASE_LOG_H
