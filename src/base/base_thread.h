#ifndef BASE_THREAD_H
#define BASE_THREAD_H

#include "base_memory.h"
#include "base_os_linux.h"

typedef struct ThreadContext ThreadContext;
struct ThreadContext {
    u32 thread_id;
    Arena *permanent_arena;
    Scratch *last_free_scratch_arena;
    IO_Uring ring;
    OS_Handle server_handle;
};

ThreadContext *thread_context_alloc(u32 thread_id);
void thread_context_release(ThreadContext *context);

#endif // BASE_THREAD_H
