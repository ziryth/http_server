#ifndef BASE_STRING_H
#define BASE_STRING_H

#include "base_core.h"

//////////////////////////////
// String type

typedef struct {
    u64 len;
    u8 *data;
} String8;

//////////////////////////////
// String functions

#define str8(string)                       \
    (String8) {                            \
        sizeof(string) - 1, (u8 *)(string) \
    }

#define str8_expand(s) (int)(s.len), (s.data)

b32 str8_is_valid(String8 string);
b32 str8_is_in_bounds(String8 source, u64 pos);
b32 str8_are_equal(String8 a, String8 b);
String8 str8_allocate(u64 len);

String8 str8_prefix(String8 string, u64 size);
String8 str8_postfix(String8 string, u64 size);
String8 str8_skip(String8 string, u64 amount);
String8 str8_split_to(String8 string, u8 *delimiter);
String8 str8_read_to(String8 *string, u8 *delimiter);

i64 str8_find_substring(String8 string, u8 *substring);

#endif // BASE_STRING_H
