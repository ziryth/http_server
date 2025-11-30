#include "base/base_inc.h"
#include "base/base_os_linux.h"

#define REQUEST_BUFFER_SIZE 8192

enum EventType {
    EventType_Accept,
    EventType_Read,
    EventType_Write,
};

struct Request {
    enum EventType event_type;

    OS_Handle client_handle;
    SockAddrIPv4 client_address;
    u64 client_address_length;

    u32 request_buffer_count;
    u8 request_buffer[REQUEST_BUFFER_SIZE];

    u32 response_buffer_count;
    u8 response_buffer[REQUEST_BUFFER_SIZE];
};

b32 submit_read(IO_Uring *ring, struct Request *request) {
    u32 tail;
    b32 ok = 0;
    IO_Uring_Submission_Entry *sqe = os_io_uring_get_sqe(ring, &tail);

    os_io_uring_prep_sqe(sqe, IORING_OP_READ);

    sqe->fd = request->client_handle.value;
    sqe->addr = (unsigned long)request->request_buffer;
    sqe->len = request->request_buffer_count;
    sqe->off = -1;
    sqe->user_data = (u64)request;

    os_io_write_barrier(ring->sring_tail, tail + 1);

    i32 result = os_io_uring_enter(ring->ring_fd, 1, 0, 0);

    if (result >= 0) {
        ok = 1;
    }

    return ok;
}

b32 submit_write(IO_Uring *ring, struct Request *request) {
    u32 tail;
    b32 ok = 0;
    IO_Uring_Submission_Entry *sqe = os_io_uring_get_sqe(ring, &tail);

    os_io_uring_prep_sqe(sqe, IORING_OP_WRITE);

    sqe->fd = request->client_handle.value;
    sqe->addr = (unsigned long)request->response_buffer;
    sqe->len = request->response_buffer_count;
    sqe->off = -1;
    sqe->user_data = (u64)request;

    os_io_write_barrier(ring->sring_tail, tail + 1);

    u32 result = os_io_uring_enter(ring->ring_fd, 1, 0, 0);

    if (result >= 0) {
        ok = 1;
    }

    return ok;
}

b32 submit_accept(Arena *arena,
                  IO_Uring *ring,
                  u32 server_fd) {
    u32 tail;
    b32 ok = 0;
    IO_Uring_Submission_Entry *sqe = os_io_uring_get_sqe(ring, &tail);
    struct Request *request = arena_push(arena, sizeof(struct Request), 8);
    request->request_buffer_count = REQUEST_BUFFER_SIZE;
    request->event_type = EventType_Accept;
    request->client_address_length = sizeof(SockAddrIPv4);

    os_io_uring_prep_sqe(sqe, IORING_OP_ACCEPT);

    sqe->fd = server_fd;
    sqe->addr = (u64)&request->client_address;
    sqe->addr2 = (u64)&request->client_address_length;
    sqe->user_data = (u64)request;

    os_io_write_barrier(ring->sring_tail, tail + 1);

    int ret = os_io_uring_enter(ring->ring_fd, 1, 0, 0);

    if (ret >= 0) {
        ok = 1;
    }

    return ok;
}

void handle_request(IO_Uring *ring, struct Request *request) {
    const char *http_response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 12\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Hello World!";

    log_info("GOT REQUEST: %s\n", request->request_buffer);

    memcpy(request->response_buffer, http_response, strlen(http_response));
    request->response_buffer_count = strlen(http_response);

    submit_write(ring, request);
}

void entrypoint(Arena *arena, OS_Handle server_handle) {
    IO_Uring_Completion_Entry *cqe;
    IO_Uring ring;

    if (os_io_uring_init_ring(&ring)) {
        log_fatal("Failed to initialize io_uring - %d\n");
        os_abort(1);
    }

    submit_accept(arena, &ring, server_handle.value);

    for (;;) {
        i32 result = os_io_uring_wait_cqe(&ring, &cqe);

        if (result != 0) {
            log_fatal("Error while getting entry from completion queue\n");
            os_abort(1);
        }

        struct Request *request = (struct Request *)cqe->user_data;

        switch (request->event_type) {
        case EventType_Accept:
            log_info("Received accept - %d \n", request->client_address.addr);
            submit_accept(arena, &ring, server_handle.value);
            request->event_type = EventType_Read;
            request->client_handle = os_handle_from_fd(cqe->res);
            submit_read(&ring, request);
            break;
        case EventType_Read:
            log_info("Received read!\n");
            request->event_type = EventType_Write;
            handle_request(&ring, request);
            // TODO: process request header
            break;
        case EventType_Write:
            log_info("Received write!\n");
            os_close(request->client_handle);
            break;
        default:
            log_info("receives something else\n");
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
    Arena *arena = arena_alloc(8 * megabyte, 64 * kilobyte, 0, 1);

    u32 port = 8080;
    u32 backlog = 3;

    OS_Handle server_handle = bind_and_listen(port, backlog);

    if (server_handle.value == 0) {
        log_fatal("failed to bind and listen to port %d\n", port);
        os_abort(1);
    }

    log_info("Starting server - listening on port %d\n", port);

    entrypoint(arena, server_handle);

    return 0;
}
