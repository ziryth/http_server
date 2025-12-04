#include "base_thread.h"
#include "base_core.h"
#include "base_memory.h"

#define ARENA_RESERVE_SIZE (64 * megabyte)
#define ARENA_COMMIT_SIZE (64 * kilobyte)
#define SCRATCH_BUFFER_SIZE (32 * kilobyte)

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

Scratch *thread_scratch_alloc(ThreadContext *context) {
    Scratch *result = 0;

    if (context->last_free_scratch_arena) {
        result = context->last_free_scratch_arena;
        context->last_free_scratch_arena = result->next_free;
    } else {
        void *backing_buffer = arena_push_zero(context->permanent_arena, SCRATCH_BUFFER_SIZE, 8);
        result = arena_alloc(SCRATCH_BUFFER_SIZE, SCRATCH_BUFFER_SIZE, backing_buffer, 0);
    }

    return result;
}

void thread_scratch_release(ThreadContext *context, Scratch *scratch) {
    arena_clear(scratch);
    scratch->next_free = context->last_free_scratch_arena;
    context->last_free_scratch_arena = scratch;
}
