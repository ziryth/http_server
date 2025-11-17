#ifndef BASE_BUFFER_H
#define BASE_BUFFER_H

#include "base_core.h"

//////////////////////////////
// Buffer

typedef struct {
    u64 len;
    u8 *data;
} Buffer;

#define String8(string)                    \
    (Buffer) {                             \
        sizeof(string) - 1, (u8 *)(string) \
    }

b32 is_valid(Buffer buffer);
b32 is_in_bounds(Buffer source, u64 pos);
b32 are_equal(Buffer a, Buffer b);
Buffer allocate_buffer(u64 len);

#endif // BASE_BUFFER_H
