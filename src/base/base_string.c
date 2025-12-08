#include "base_string.h"
#include "base_core.h"
#include <string.h>

b32 str8_is_valid(String8 string) {
    b32 result = (string.data != 0);
    return result;
}

b32 str8_is_in_bounds(String8 source, u64 pos) {
    b32 result = (pos < source.len);
    return result;
}

b32 str8_are_equal(String8 a, String8 b) {
    if (a.len != b.len) {
        return 0;
    }

    for (u64 index = 0; index < a.len; ++index) {
        if (a.data[index] != b.data[index]) {
            return 0;
        }
    }

    return 1;
}

String8 str8_prefix(String8 string, u64 size) {
    u64 size_clamped = ClampTop(size, string.len);
    String8 result = {size_clamped, string.data};

    return result;
}

String8 str8_postfix(String8 string, u64 size) {
    u64 size_clamped = ClampTop(size, string.len);
    u64 new_base = string.len - size_clamped;
    String8 result = {size, string.data + new_base};

    return result;
}

String8 str8_skip(String8 string, u64 amount) {
    u64 amount_clamped = ClampTop(amount, string.len);
    String8 result = {string.len - amount_clamped, string.data + amount_clamped};

    return result;
}

String8 str8_split_to(String8 string, u8 *delimiter) {
    String8 result = {0};

    i64 delimiter_pos = str8_find_substring(string, delimiter);

    if (delimiter_pos == -1) {
        return result;
    }

    result = str8_prefix(string, delimiter_pos);

    return result;
}

String8 str8_read_to(String8 *string, u8 *delimiter) {
    String8 result = str8_split_to(*string, delimiter);
    string->data += result.len + strlen(delimiter);

    return result;
}

i64 str8_find_substring(String8 string, u8 *substring) {
    if (!*substring) {
        return -1;
    }

    for (i64 pos = 0; pos < string.len; pos++) {
        const u8 *h = string.data + pos;
        const u8 *n = substring;

        while (*h && *n && *h == *n) {
            h++;
            n++;
        }

        if (!*n) {
            return pos;
        }
    }

    return -1;
}
