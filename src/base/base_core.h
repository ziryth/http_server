#ifndef BASE_CORE_H
#define BASE_CORE_H

#include "stdint.h"
#include <stdarg.h>

//////////////////////////////
// Basic Types

typedef char u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

typedef uint32_t b32;

//////////////////////////////
// Helper Macros

#define array_count(a) (sizeof(a) / sizeof(*(a)))

#define global static
#define local static

#define AlignPow2(x, b) (((x) + (b) - 1) & (~((b) - 1)))

#define Min(a, b) (((a) < (b)) ? (a) : (b))
#define Max(a, b) (((a) > (b)) ? (a) : (b))
#define ClampTop(x, top) Min((x), (top))
#define ClampBottom(x, bottom) Max((x), (bottom))

//////////////////////////////
// Basic Constants

global u64 kilobyte = 1024;
global u64 megabyte = 1024 * 1024;
global u64 gigabyte = 1024 * 1024 * 1024;

//////////////////////////////
// System Calls

static inline i64 syscall0(i64 n) {
    i64 result;
    asm volatile("syscall"
                 : "=a"(result)
                 : "a"(n)
                 : "rcx", "r11", "memory");
    return result;
}

static inline i64 syscall1(i64 n, i64 a1) {
    i64 result;
    asm volatile("syscall"
                 : "=a"(result)
                 : "a"(n), "D"(a1)
                 : "rcx", "r11", "memory");
    return result;
}

static inline i64 syscall2(i64 n, i64 a1, i64 a2) {
    i64 result;
    asm volatile("syscall"
                 : "=a"(result)
                 : "a"(n), "D"(a1), "S"(a2)
                 : "rcx", "r11", "memory");
    return result;
}

static inline i64 syscall3(i64 n, i64 a1, i64 a2, i64 a3) {
    i64 result;
    asm volatile("syscall"
                 : "=a"(result)
                 : "a"(n), "D"(a1), "S"(a2), "d"(a3)
                 : "rcx", "r11", "memory");
    return result;
}

static inline i64 syscall4(i64 n, i64 a1, i64 a2, i64 a3, i64 a4) {
    i64 result;
    register i64 r10 asm("r10") = a4;
    asm volatile("syscall"
                 : "=a"(result)
                 : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10)
                 : "rcx", "r11", "memory");
    return result;
}

static inline i64 syscall5(i64 n, i64 a1, i64 a2, i64 a3, i64 a4, i64 a5) {
    i64 result;
    register i64 r10 asm("r10") = a4;
    register i64 r8 asm("r8") = a5;
    asm volatile("syscall"
                 : "=a"(result)
                 : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8)
                 : "rcx", "r11", "memory");
    return result;
}

static inline i64 syscall6(
    i64 n, i64 a1, i64 a2, i64 a3, i64 a4, i64 a5, i64 a6) {
    i64 result;
    register i64 r10 asm("r10") = a4;
    register i64 r8 asm("r8") = a5;
    register i64 r9 asm("r9") = a6;
    asm volatile("syscall"
                 : "=a"(result)
                 : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
                 : "rcx", "r11", "memory");
    return result;
}

#endif // BASE_CORE_H
