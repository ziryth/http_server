#include "base_memory.h"
#include "base_core.h"
#include "base_log.h"
#include "base_os_linux.h"
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

//////////////////////////////
// Primitives

void *mem_reserve(u64 size) {
    void *result = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);

    return result;
}

void *mem_allocate(u64 size) {
    void *result = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);

    return result;
}

i32 mem_commit(void *ptr, u64 size) {
    return mprotect(ptr, size, PROT_READ | PROT_WRITE);
}

i32 mem_decommit(void *ptr, u64 size) {
    return madvise(ptr, size, MADV_DONTNEED);
}
i32 mem_release(void *ptr, u64 size) {
    return munmap(ptr, size);
}

//////////////////////////////
// Arena

Arena *arena_alloc(u64 initial_size) {
    Arena *arena = mem_allocate(sizeof(Arena));

    void *ptr = mem_reserve(RESERVE_SIZE);

    if (ptr == 0) {
        arena_error((u8 *)"Failed to reserve address space");
    }

    arena->base_ptr = ptr;
    arena->pos = 0;
    arena->size = RESERVE_SIZE;
    arena->committed = initial_size;

    if (mem_commit(arena->base_ptr, initial_size) != 0) {
        arena_error((u8 *)"Failed to commit pages initial pages");
    }

    return arena;
}

void arena_release(Arena *arena) {
    if (mem_release(arena->base_ptr, arena->size) != 0) {
        arena_error((u8 *)"Failed to release arena memory");
    }
    free(arena);
}

void *arena_push(Arena *arena, u64 size) {
    void *ptr = (char *)arena->base_ptr + arena->pos;

    if (arena->pos + size > arena->committed) {
        u64 new_committed = arena->committed * 2;
        u64 commit_size = new_committed - arena->committed;
        if (mem_commit(arena->base_ptr + arena->committed, commit_size) != 0) {
            arena_error((u8 *)"Failed to grow arena");
        }
        arena->committed = new_committed;
    }
    arena->pos += size;

    return ptr;
}

void *arena_push_zero(Arena *arena, u64 size) {
    void *ptr = arena_push(arena, size);
    memset(ptr, 0, size);

    return ptr;
}

void arena_pop(Arena *arena, u64 size) {
    if (size > arena->pos) {
        arena_error((u8 *)"Popping to invalid position");
    }

    arena->pos -= size;
}
void arena_pop_to(Arena *arena, u64 pos) {
    if (pos > arena->pos || pos < 0) {
        arena_error((u8 *)"Popping to invalid position");
    }

    arena->pos = pos;
}

u64 arena_get_pos(Arena *arena) {
    return arena->pos;
}

void arena_clear(Arena *arena) {
    arena->pos = 0;
}

void arena_error(u8 *message) {
    log_error((char *)message);
    os_abort(1);
}
