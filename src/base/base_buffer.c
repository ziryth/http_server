#include "base_buffer.h"
#include "base_core.h"
#include "base_memory.h"
#include <sys/mman.h>

b32 is_valid(Buffer buffer) {
    b32 result = (buffer.data != 0);
    return result;
}

b32 is_in_bounds(Buffer source, u64 pos) {
    b32 result = (pos < source.len);
    return result;
}

b32 are_equal(Buffer a, Buffer b) {
    if (a.len != b.len) {
        return 0;
    }

    for (u64 index = 0; index < a.len; ++index) {
        if (a.data[index] != b.data[index]) {
            return 1;
        }
    }

    return 1;
}

Buffer allocate_buffer(u64 len) {
    Buffer result = {0};
    result.data = mem_allocate(len);

    if (result.data) {
        result.len = len;
    }

    return result;
}
