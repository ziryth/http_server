#include "base_memory.h"
#include "base_core.h"
#include "base_log.h"
#include "base_os_linux.h"
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

Arena *arena_alloc(u64 reserve_size, u64 commit_size, void *optional_buffer, b32 is_chained) {
    reserve_size = AlignPow2(reserve_size, PAGE_SIZE);
    commit_size = AlignPow2(commit_size, PAGE_SIZE);

    void *base_pointer = optional_buffer;

    if (base_pointer == 0) {
        base_pointer = mem_reserve(reserve_size);
        mem_commit(base_pointer, commit_size);
    }

    if (base_pointer == 0) {
        log_error("Failed to allocate memory region\n");
        os_abort(1);
    }

    Arena *arena = (Arena *)base_pointer;
    arena->current = arena;
    arena->base_pointer = base_pointer;
    arena->pos = ARENA_HEADER_SIZE;
    arena->reserve_size = reserve_size;
    arena->commit_size = commit_size;
    arena->is_chained = is_chained;
    arena->committed = commit_size;

    return arena;
}

void *arena_push(Arena *arena, u64 size, u64 align) {
    Arena *current = arena->current;

    u64 pos_new = AlignPow2(current->pos, align) + size;

    if (pos_new > current->reserve_size && current->is_chained) {
        u64 reserve_size = current->reserve_size;
        u64 commit_size = current->commit_size;

        if (ARENA_HEADER_SIZE + size > reserve_size) {
            reserve_size = AlignPow2(ARENA_HEADER_SIZE + size, align);
            commit_size = AlignPow2(ARENA_HEADER_SIZE + size, align);
        }

        Arena *new_arena_block = arena_alloc(reserve_size, commit_size, 0, current->is_chained);

        new_arena_block->prev = arena->current;
        arena->current = new_arena_block;
        current = new_arena_block;
        pos_new = AlignPow2(current->pos, align) + size;
    }

    if (current->committed < pos_new) {
        u64 commit_aligned = pos_new + current->commit_size - 1;
        commit_aligned -= commit_aligned % current->commit_size;
        u64 commit_clamped = ClampTop(commit_aligned, current->reserve_size);
        u64 commit_size = commit_clamped - current->committed;
        void *commit_base = (u8 *)current + current->committed;

        mem_commit(commit_base, commit_size);
        current->committed = commit_clamped;
    }

    void *result = current->base_pointer + current->pos;
    current->pos = pos_new;

    return result;
}

void *arena_push_zero(Arena *arena, u64 size, u64 align) {
    void *ptr = arena_push(arena, size, align);
    memset(ptr, 0, size);

    return ptr;
}

// void arena_release(Arena *arena) {
//     if (mem_release(arena->base_ptr, arena->size) != 0) {
//         arena_error((u8 *)"Failed to release arena memory");
//     }
//     free(arena);
// }
//
//
//
// void arena_pop(Arena *arena, u64 size) {
//     if (size > arena->pos) {
//         arena_error((u8 *)"Popping to invalid position");
//     }
//
//     arena->pos -= size;
// }
// void arena_pop_to(Arena *arena, u64 pos) {
//     if (pos > arena->pos || pos < 0) {
//         arena_error((u8 *)"Popping to invalid position");
//     }
//
//     arena->pos = pos;
// }
//
// u64 arena_get_pos(Arena *arena) {
//     return arena->pos;
// }
//
// void arena_clear(Arena *arena) {
//     arena->pos = 0;
// }
