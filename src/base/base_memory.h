#ifndef BASE_MEMORY_H
#define BASE_MEMORY_H

#include "base_core.h"

#define RESERVE_SIZE 64ULL * 1024 * 1024 * 1024

//////////////////////////////
// Primitives

void *mem_reserve(u64 size);
void *mem_allocate(u64 size);
i32 mem_commit(void *ptr, u64 size);
i32 mem_decommit(void *ptr, u64 size);
i32 mem_release(void *ptr, u64 size);

void mem_error(u8 *message);

//////////////////////////////
// Arena

typedef struct {
    void *base_ptr;
    u64 size;
    u64 pos;
    u64 committed;
} Arena;

Arena *arena_alloc(u64 initial_size);

void arena_release(Arena *arena);

void *arena_push(Arena *arena, u64 size);
void *arena_push_zero(Arena *arena, u64 size);

#define push_array(arena, type, count) (type *)arena_push((arena), sizeof(type) * (count))
#define push_array_zero(arena, type, count) (type *)arena_push_zero((arena), sizeof(type) * (count))
#define push_struct(arena, type) push_array((arena), (type), 1)
#define push_struct_zero(arena, type) push_array_zero((arena), (type), 1)

void arena_pop(Arena *arena, u64 size);
void arena_pop_to(Arena *arena, u64 pos);
u64 arena_get_pos(Arena *arena);
void arena_clear(Arena *arena);
void arena_error(u8 *message);

#endif // BASE_MEMORY_H
