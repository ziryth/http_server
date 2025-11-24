#ifndef BASE_BUFFER_H
#define BASE_BUFFER_H

#include "base_core.h"

//////////////////////////////
// Buffer

typedef struct {
    u64 len;
    u8 *data;
} Buffer;

#define str8(string)                       \
    (Buffer) {                             \
        sizeof(string) - 1, (u8 *)(string) \
    }

b32 buffer_is_valid(Buffer buffer);
b32 buffer_is_in_bounds(Buffer source, u64 pos);
b32 buffer_are_equal(Buffer a, Buffer b);
Buffer buffer_allocate(u64 len);

#endif // BASE_BUFFER_H
