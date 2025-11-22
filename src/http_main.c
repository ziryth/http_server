#include "base/base_inc.h"
#include "base/base_memory.h"
#include "base/base_os_linux.h"
#include <linux/io_uring.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define QUEUE_DEPTH 1
#define REQUEST_BUFFER_SIZE 8192

enum EventType {
    EventType_Accept,
    EventType_Read,
    EventType_Write,
};

struct IOUringContext {
    i32 ring_fd;
    u32 *sring_tail;
    u32 *sring_mask;
    u32 *sring_array;
    u32 *cring_head;
    u32 *cring_tail;
    u32 *cring_mask;

    struct io_uring_sqe *sqes;
    struct io_uring_cqe *cqes;

    off_t offset;
};

struct Request {
    enum EventType event_type;
    u32 client_socket;
    u8 buffer[REQUEST_BUFFER_SIZE];
};

i32 setup_io_uring(struct IOUringContext *ring) {
    struct io_uring_params p = {0};
    void *sq_ptr, *cq_ptr;

    ring->ring_fd = os_io_uring_setup(QUEUE_DEPTH, &p);

    if (ring->ring_fd < 0) {
        perror("io_uring_setup");
        return 1;
    }

    /*
     * io_uring communication happens via 2 shared kernel-user space ring
     * buffers, which can be jointly mapped with a single mmap() call in
     * kernels >= 5.4.
     */

    int sring_sz = p.sq_off.array + p.sq_entries * sizeof(unsigned);
    int cring_sz = p.cq_off.cqes + p.cq_entries * sizeof(struct io_uring_cqe);

    /* Rather than check for kernel version, the recommended way is to
     * check the features field of the io_uring_params structure, which is a
     * bitmask. If IORING_FEAT_SINGLE_MMAP is set, we can do away with the
     * second mmap() call to map in the completion ring separately.
     */
    if (p.features & IORING_FEAT_SINGLE_MMAP) {
        if (cring_sz > sring_sz)
            sring_sz = cring_sz;
        cring_sz = sring_sz;
    }

    /* Map in the submission and completion queue ring buffers.
     *  Kernels < 5.4 only map in the submission queue, though.
     */
    sq_ptr = mmap(0, sring_sz, PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_POPULATE,
                  ring->ring_fd, IORING_OFF_SQ_RING);
    if (sq_ptr == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    if (p.features & IORING_FEAT_SINGLE_MMAP) {
        cq_ptr = sq_ptr;
    } else {
        /* Map in the completion queue ring buffer in older kernels separately */
        cq_ptr = mmap(0, cring_sz, PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_POPULATE,
                      ring->ring_fd, IORING_OFF_CQ_RING);
        if (cq_ptr == MAP_FAILED) {
            perror("mmap");
            return 1;
        }
    }
    /* Save useful fields for later easy reference */
    ring->sring_tail = sq_ptr + p.sq_off.tail;
    ring->sring_mask = sq_ptr + p.sq_off.ring_mask;
    ring->sring_array = sq_ptr + p.sq_off.array;

    /* Map in the submission queue entries array */
    ring->sqes = mmap(0, p.sq_entries * sizeof(struct io_uring_sqe),
                      PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
                      ring->ring_fd, IORING_OFF_SQES);
    if (ring->sqes == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    /* Save useful fields for later easy reference */
    ring->cring_head = cq_ptr + p.cq_off.head;
    ring->cring_tail = cq_ptr + p.cq_off.tail;
    ring->cring_mask = cq_ptr + p.cq_off.ring_mask;
    ring->cqes = cq_ptr + p.cq_off.cqes;

    return 0;
}

i32 io_uring_wait_cqe(struct IOUringContext *ring, struct io_uring_cqe **cqe) {
    u32 head;

    i32 result = os_io_uring_enter(ring->ring_fd, 0, 1, IORING_ENTER_GETEVENTS);

    head = os_io_read_barrier(ring->cring_head);

    // TODO: do we even need this check?? io_uring_enter should guarantee
    if (head == *ring->cring_tail)
        return -1;

    *cqe = &ring->cqes[head & (*ring->cring_mask)];
    head++;

    os_io_write_barrier(ring->cring_head, head);

    return result;
}

struct io_uring_sqe *io_uring_get_sqe(struct IOUringContext *ring, u32 *tail_out) {
    u32 tail = os_io_read_barrier(ring->sring_tail);
    u32 index = tail & *ring->sring_mask;

    struct io_uring_sqe *sqe = &ring->sqes[index];
    *tail_out = tail;

    return sqe;
}

void io_uring_prep_sqe(struct io_uring_sqe *sqe, u32 opcode) {
    memset(sqe, 0, sizeof(*sqe));
    sqe->opcode = opcode;
}

i32 submit_read(struct IOUringContext *ring, struct Request *request) {
    u32 tail;

    struct io_uring_sqe *sqe = io_uring_get_sqe(ring, &tail);

    io_uring_prep_sqe(sqe, IORING_OP_READ);

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

i32 submit_write(struct IOUringContext *ring, struct Request *request) {
    u32 tail;

    struct io_uring_sqe *sqe = io_uring_get_sqe(ring, &tail);

    io_uring_prep_sqe(sqe, IORING_OP_WRITE);

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
                   struct IOUringContext *ring,
                   u32 server_fd,
                   SockAddrIPv4 *client_addr,
                   u64 *client_addr_len) {
    u32 tail;
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring, &tail);
    struct Request *request = arena_push(arena, sizeof(struct Request));
    request->event_type = EventType_Accept;

    io_uring_prep_sqe(sqe, IORING_OP_ACCEPT);

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

i32 setup_server_socket(u32 port, u32 backlog) {
    i32 server_fd = os_socket_ipv4();
    if (server_fd < 0) {
        printf("ERROR - socket failed: %d\n", server_fd);
        return -1;
    }

    i32 bind_result = os_bind_ipv4(server_fd, port);
    if (bind_result < 0) {
        printf("ERROR - bind failed: %d\n", bind_result);
        return -1;
    }

    i32 listen_result = os_listen(server_fd, backlog);
    if (listen_result < 0) {
        printf("ERROR - listen failed: %d\n", listen_result);
        return -1;
    }

    return server_fd;
}

void entrypoint(Arena *arena, i32 server_fd) {
    struct io_uring_cqe *cqe;
    struct IOUringContext ring;

    // TODO: This will be a problem with io_uring async - probably needs to be baked into the request
    SockAddrIPv4 client_addr = {0};
    u64 client_addr_len = sizeof(client_addr);

    i32 result = setup_io_uring(&ring);

    submit_accept(arena, &ring, server_fd, &client_addr, &client_addr_len);

    for (;;) {
        i32 result = io_uring_wait_cqe(&ring, &cqe);
        struct Request *request = (struct Request *)cqe->user_data;

        switch (request->event_type) {
        case EventType_Accept:
            printf("Received accept!\n");
            submit_accept(arena, &ring, server_fd, &client_addr, &client_addr_len);
            request->event_type = EventType_Read;
            request->client_socket = cqe->res;
            submit_read(&ring, request);
            break;
        case EventType_Read:
            printf("Received read!\n");
            request->event_type = EventType_Write;
            submit_write(&ring, request);
            // TODO: process request header
            break;
        case EventType_Write:
            printf("Received write!\n");
            os_close(request->client_socket);
            break;
        default:
            printf("receives something else\n");
            break;
        };
    }
}

i32 main(void) {
    Arena *arena = arena_alloc(1024 * 1024);

    u32 port = 8081;
    u32 backlog = 3;
    i32 server_fd = setup_server_socket(port, backlog);

    entrypoint(arena, server_fd);

    return 0;
}
