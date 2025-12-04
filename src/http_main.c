#include "base/base_inc.h"
#include "base/base_memory.h"
#include "base/base_os_linux.h"
#include "base/base_thread.h"

#define REQUEST_BUFFER_SIZE 8192

enum EventType {
    EventType_Accept,
    EventType_Read,
    EventType_Write,
};

struct Request {
    enum EventType event_type;

    Scratch *scratch_arena;

    OS_Handle client_handle;
    SockAddrIPv4 client_address;
    u64 client_address_length;

    Buffer request_buffer;
    Buffer response_buffer;
};

b32 submit_read(ThreadContext *context, struct Request *request) {
    u32 tail;
    b32 ok = 0;
    Scratch *scratch = request->scratch_arena;
    IO_Uring_Submission_Entry *sqe = os_io_uring_get_sqe(&context->ring, &tail);

    Buffer request_buffer = {0};
    request_buffer.data = arena_push(scratch, REQUEST_BUFFER_SIZE, 8);
    request_buffer.len = REQUEST_BUFFER_SIZE;
    request->request_buffer = request_buffer;

    os_io_uring_prep_sqe(sqe, IORING_OP_READ);

    sqe->fd = request->client_handle.value;
    sqe->addr = (u64)request->request_buffer.data;
    sqe->len = request->request_buffer.len;
    sqe->off = -1;
    sqe->user_data = (u64)request;

    os_io_write_barrier(context->ring.sring_tail, tail + 1);

    i32 result = os_io_uring_enter(context->ring.ring_fd, 1, 0, 0);

    if (result >= 0) {
        ok = 1;
    }

    return ok;
}

b32 submit_write(ThreadContext *context, struct Request *request) {
    u32 tail;
    b32 ok = 0;
    IO_Uring_Submission_Entry *sqe = os_io_uring_get_sqe(&context->ring, &tail);

    os_io_uring_prep_sqe(sqe, IORING_OP_WRITE);

    sqe->fd = request->client_handle.value;
    sqe->addr = (u64)request->response_buffer.data;
    sqe->len = request->response_buffer.len;
    sqe->off = -1;
    sqe->user_data = (u64)request;

    os_io_write_barrier(context->ring.sring_tail, tail + 1);

    u32 result = os_io_uring_enter(context->ring.ring_fd, 1, 0, 0);

    if (result >= 0) {
        ok = 1;
    }

    return ok;
}

b32 submit_accept(ThreadContext *context) {
    u32 tail;
    b32 ok = 0;
    IO_Uring_Submission_Entry *sqe = os_io_uring_get_sqe(&context->ring, &tail);
    Scratch *scratch = thread_scratch_alloc(context);
    struct Request *request = arena_push(scratch, sizeof(struct Request), 8);
    request->event_type = EventType_Accept;
    request->client_address_length = sizeof(SockAddrIPv4);
    request->scratch_arena = scratch;

    os_io_uring_prep_sqe(sqe, IORING_OP_ACCEPT);

    sqe->fd = context->server_handle.value;
    sqe->addr = (u64)&request->client_address;
    sqe->addr2 = (u64)&request->client_address_length;
    sqe->user_data = (u64)request;

    os_io_write_barrier(context->ring.sring_tail, tail + 1);

    u32 result = os_io_uring_enter(context->ring.ring_fd, 1, 0, 0);

    if (result >= 0) {
        ok = 1;
    }

    return ok;
}

void handle_request(ThreadContext *context, struct Request *request) {
    Buffer http_response = str8(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 12\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Hello World!");

    request->response_buffer = http_response;

    log_info("GOT REQUEST: %s\n", request->request_buffer.data);

    submit_write(context, request);
}

void entrypoint(u32 thread_id, OS_Handle server_handle) {
    IO_Uring_Completion_Entry *cqe;
    ThreadContext *context = thread_context_alloc(thread_id);
    context->server_handle = server_handle;

    if (os_io_uring_init_ring(&context->ring)) {
        log_fatal("Failed to initialize io_uring - %d\n");
        os_abort(1);
    }

    submit_accept(context);

    for (;;) {
        i32 result = os_io_uring_wait_cqe(&context->ring, &cqe);

        if (result != 0) {
            log_fatal("Error while getting entry from completion queue\n");
            os_abort(1);
        }

        struct Request *request = (struct Request *)cqe->user_data;

        switch (request->event_type) {
        case EventType_Accept:
            submit_accept(context);
            request->event_type = EventType_Read;
            request->client_handle = os_handle_from_fd(cqe->res);
            submit_read(context, request);
            break;
        case EventType_Read:
            request->event_type = EventType_Write;
            handle_request(context, request);
            break;
        case EventType_Write:
            os_close(request->client_handle);
            thread_scratch_release(context, request->scratch_arena);
            break;
        default:
            break;
        };
    }
}

OS_Handle bind_and_listen(u32 port, u32 backlog) {
    OS_Handle handle = os_socket_ipv4();

    if (!os_bind_ipv4(handle, port)) {
        log_fatal("failed to bind to port %d\n", port);
        os_abort(1);
    }

    if (!os_listen(handle, backlog)) {
        log_fatal("listen failed\n");
        os_abort(1);
    }

    return handle;
}

i32 main(void) {
    u32 port = 8080;
    u32 backlog = 3;
    OS_Handle server_handle = bind_and_listen(port, backlog);

    if (server_handle.value == 0) {
        log_fatal("failed to bind and listen to port %d\n", port);
        os_abort(1);
    }

    log_info("Starting server - listening on port %d\n", port);

    entrypoint(0, server_handle);

    return 0;
}
