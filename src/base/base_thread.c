#include "base_thread.h"
#include "base_core.h"
#include "base_memory.h"

#define ARENA_RESERVE_SIZE (64 * megabyte)
#define ARENA_COMMIT_SIZE (64 * kilobyte)

ThreadContext *thread_context_alloc(u32 thread_id) {
    Arena *arena = arena_alloc(ARENA_RESERVE_SIZE, ARENA_COMMIT_SIZE, 0, 1);

    ThreadContext *result = arena_push_zero(arena, sizeof(ThreadContext), sizeof(ThreadContext));
    result->thread_id = 0;
    result->permanent_arena = arena;

    return result;
}

void thread_context_release(ThreadContext *context) {
    Arena *arena = context->permanent_arena;

    arena_release(arena);
}
