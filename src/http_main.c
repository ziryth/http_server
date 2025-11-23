#include "base/base_inc.h"
#include "base/base_memory.h"
#include "base/base_os_linux.h"
#include <linux/io_uring.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#define REQUEST_BUFFER_SIZE 8192

enum EventType {
    EventType_Accept,
    EventType_Read,
    EventType_Write,
};

struct Request {
    enum EventType event_type;
    u32 client_socket;
    u8 buffer[REQUEST_BUFFER_SIZE];
};

i32 submit_read(IO_Uring *ring, struct Request *request) {
    u32 tail;

    IO_Uring_Submission_Entry *sqe = os_io_uring_get_sqe(ring, &tail);

    os_io_uring_prep_sqe(sqe, IORING_OP_READ);

    sqe->fd = request->client_socket;
    sqe->addr = (unsigned long)request->buffer;
    sqe->len = REQUEST_BUFFER_SIZE;
    sqe->off = -1;
    sqe->user_data = (u64)request;

    os_io_write_barrier(ring->sring_tail, tail + 1);

    int ret = os_io_uring_enter(ring->ring_fd, 1, 0, 0);

    if (ret < 0) {
        perror("io_uring_enter");
        return -1;
    }

    return ret;
}

i32 submit_write(IO_Uring *ring, struct Request *request) {
    u32 tail;

    IO_Uring_Submission_Entry *sqe = os_io_uring_get_sqe(ring, &tail);

    os_io_uring_prep_sqe(sqe, IORING_OP_WRITE);

    sqe->fd = request->client_socket;
    sqe->addr = (unsigned long)request->buffer;
    sqe->len = REQUEST_BUFFER_SIZE;
    sqe->off = -1;
    sqe->user_data = (u64)request;

    os_io_write_barrier(ring->sring_tail, tail + 1);

    int ret = os_io_uring_enter(ring->ring_fd, 1, 0, 0);

    if (ret < 0) {
        perror("io_uring_enter");
        return -1;
    }

    return ret;
}

void submit_accept(Arena *arena,
                   IO_Uring *ring,
                   u32 server_fd,
                   SockAddrIPv4 *client_addr,
                   u64 *client_addr_len) {
    u32 tail;
    IO_Uring_Submission_Entry *sqe = os_io_uring_get_sqe(ring, &tail);
    struct Request *request = arena_push(arena, sizeof(struct Request));
    request->event_type = EventType_Accept;

    os_io_uring_prep_sqe(sqe, IORING_OP_ACCEPT);

    sqe->fd = server_fd;
    sqe->addr = (u64)client_addr;
    sqe->addr2 = (u64)client_addr_len;
    sqe->user_data = (u64)request;

    os_io_write_barrier(ring->sring_tail, tail + 1);

    int ret = os_io_uring_enter(ring->ring_fd, 1, 0, 0);

    if (ret < 0) {
        perror("io_uring_enter");
        return;
    }
}

void entrypoint(Arena *arena, i32 server_fd) {
    IO_Uring_Completion_Entry *cqe;
    IO_Uring ring;

    // TODO: This will be a problem with io_uring async - probably needs to be baked into the request
    SockAddrIPv4 client_addr = {0};
    u64 client_addr_len = sizeof(client_addr);

    if (os_io_uring_init_ring(&ring)) {
        log_fatal("Failed to initialize io_uring - %d\n");
        os_abort(1);
    }

    submit_accept(arena, &ring, server_fd, &client_addr, &client_addr_len);

    for (;;) {
        i32 result = os_io_uring_wait_cqe(&ring, &cqe);

        if (result != 0) {
            log_fatal("Error while getting entry from completion queue\n");
            os_abort(1);
        }

        struct Request *request = (struct Request *)cqe->user_data;

        switch (request->event_type) {
        case EventType_Accept:
            log_info("Received accept!\n");
            submit_accept(arena, &ring, server_fd, &client_addr, &client_addr_len);
            request->event_type = EventType_Read;
            request->client_socket = cqe->res;
            submit_read(&ring, request);
            break;
        case EventType_Read:
            log_info("Received read!\n");
            request->event_type = EventType_Write;
            submit_write(&ring, request);
            // TODO: process request header
            break;
        case EventType_Write:
            log_info("Received write!\n");
            os_close(request->client_socket);
            break;
        default:
            log_info("receives something else\n");
            break;
        };
    }
}

i32 main(void) {
    Arena *arena = arena_alloc(1024 * 1024);

    u32 port = 8080;
    u32 backlog = 3;

    log_info("Starting server on port %d\n", port);

    i32 server_fd = os_bind_and_listen(port, backlog);

    entrypoint(arena, server_fd);

    return 0;
}
