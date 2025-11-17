#include "base/base_inc.h"
#include "base/base_os_linux.h"
#include <linux/io_uring.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define QUEUE_DEPTH 1
#define BLOCK_SZ 1024

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

i32 submit_read_request(struct IOUringContext *ring, char *buff) {
    unsigned index, tail;

    /* Add our submission queue entry to the tail of the SQE ring buffer */
    tail = *ring->sring_tail;
    index = tail & *ring->sring_mask;
    struct io_uring_sqe *sqe = &ring->sqes[index];

    sqe->opcode = IORING_OP_READ;
    sqe->fd = 0;
    sqe->addr = (unsigned long)buff;
    sqe->len = 256;

    sqe->off = -1;

    ring->sring_array[index] = index;
    tail++;

    /* Update the tail */
    os_io_write_barrier(ring->sring_tail, tail);

    /*
     * Tell the kernel we have submitted events with the io_uring_enter()
     * system call. We also pass in the IORING_ENTER_GETEVENTS flag which
     * causes the io_uring_enter() call to wait until min_complete
     * (the 3rd param) events complete.
     * */
    int ret = os_io_uring_enter(ring->ring_fd, 1, 0, 0);

    if (ret < 0) {
        perror("io_uring_enter");
        return -1;
    }

    return ret;
}

void entrypoint(i32 server_fd) {
    struct io_uring_cqe *cqe;
    struct IOUringContext ring;

    // setup io_uring here..
    i32 result = setup_io_uring(&ring);

    // server loop...
    for (;;) {
        i32 result = io_uring_wait_cqe(&ring, &cqe);

        // handle accept, read, write
    }
}

i32 main(void) {
    struct io_uring_cqe *cqe;
    struct IOUringContext ring;

    printf("setting up uring\n");
    i32 result = setup_io_uring(&ring);
    if (result != 0) {
        printf("ERROR: uring setup failed\n");
    }

    char buff[256];
    memset(buff, 0, 256);

    submit_read_request(&ring, buff);

    result = io_uring_wait_cqe(&ring, &cqe);

    sleep(2);
    printf("Buffer contains: %s\n", buff);

    // printf("socket\n");
    // i32 server_fd = os_socket_ipv4();
    // if (server_fd < 0) {
    //     printf("socket failed: %d\n", server_fd);
    //     return 1;
    // }
    // printf("server_fd: %d\n", server_fd);
    //
    // printf("bind\n");
    // i32 bind_result = os_bind_ipv4(server_fd, 8082);
    // if (bind_result < 0) {
    //     printf("bind failed: %d\n", bind_result);
    //     return 1;
    // }
    //
    // printf("listen\n");
    // i32 listen_result = os_listen(server_fd, 3);
    // if (listen_result < 0) {
    //     printf("listen failed: %d\n", listen_result);
    //     return 1;
    // }
    //
    // printf("Listening on port 8082...\n");
    //
    // printf("accept\n");
    // i32 client_fd = os_accept(server_fd);
    // if (client_fd < 0) {
    //     printf("accept failed: %d\n", client_fd);
    //     return 1;
    // }
    // printf("client_fd: %d\n", client_fd);
    //
    // printf("write\n");
    // Buffer output = String8("Hello, world\n");
    // os_write(client_fd, output);

    return 0;
}
