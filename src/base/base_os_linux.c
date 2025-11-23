#include "base_os_linux.h"
#include "base_core.h"
#include "base_log.h"

//////////////////////////////
//  Process

void os_abort(i32 exit_code) {
    syscall1(SYS_EXIT, exit_code);
}

//////////////////////////////
//  Network

u16 network_byte_order(u16 n) {
    u16 result = (n >> 8) | ((n & 0xff) << 8);

    return result;
}

i32 os_socket_ipv4() {
    i32 result = syscall3(SYS_SOCKET, AF_INET, SOCK_STREAM, 0);

    return result;
}

i32 os_bind_ipv4(u32 sock_fd, u16 port) {
    SockAddrIPv4 addr = {};

    addr.family = AF_INET;
    addr.port = network_byte_order(port);
    addr.addr = 0;

    i32 result = syscall3(SYS_BIND, sock_fd, (u64)&addr, sizeof(addr));

    return result;
}

i32 os_listen(u32 sock_fd, u32 backlog) {
    i32 result = syscall2(SYS_LISTEN, sock_fd, backlog);

    return result;
}

i32 os_bind_and_listen(u32 port, u32 backlog) {
    i32 server_fd = os_socket_ipv4();
    if (server_fd < 0) {
        log_error("socket failed: %d\n", server_fd);
        return -1;
    }

    i32 bind_result = os_bind_ipv4(server_fd, port);
    if (bind_result < 0) {
        log_error("bind failed: %d\n", bind_result);
        return -1;
    }

    i32 listen_result = os_listen(server_fd, backlog);
    if (listen_result < 0) {
        log_error("listen failed: %d\n", listen_result);
        return -1;
    }

    return server_fd;
}

i32 os_accept(u32 sock_fd) {
    i32 result = syscall3(SYS_ACCEPT, sock_fd, 0, 0);

    return result;
}

i32 os_write(i32 sock_fd, Buffer buffer) {
    u64 str_ptr = (u64)buffer.data;
    u64 len = buffer.len;
    i32 result = syscall3(SYS_WRITE, sock_fd, str_ptr, len);

    return result;
}

i32 os_close(i32 fd) {
    i32 result = syscall1(SYS_CLOSE, fd);

    return result;
}

//////////////////////////////
//  IO

i32 os_io_uring_setup(u32 entries, struct io_uring_params *p) {
    i32 result = syscall2(SYS_IO_URING_SETUP, entries, (u64)p);

    return result;
}

i32 os_io_uring_enter(i32 ring_fd, u32 to_submit, u32 min_complete, u32 flags) {
    i32 result = syscall6(SYS_IO_URING_ENTER, ring_fd, to_submit,
                          min_complete, flags, 0, 0);

    return result;
}

i32 os_io_uring_init_ring(IO_Uring *ring) {
    IO_Uring_Params p = {0};
    void *sq_ptr, *cq_ptr;

    ring->ring_fd = os_io_uring_setup(IO_URING_QUEUE_DEPTH, &p);

    if (ring->ring_fd < 0) {
        log_error("io_uring_setup call failed - %d", ring->ring_fd);
        return 1;
    }

    int sring_sz = p.sq_off.array + p.sq_entries * sizeof(unsigned);
    int cring_sz = p.cq_off.cqes + p.cq_entries * sizeof(struct io_uring_cqe);

    if (p.features & IORING_FEAT_SINGLE_MMAP) {
        if (cring_sz > sring_sz)
            sring_sz = cring_sz;
        cring_sz = sring_sz;
    }

    sq_ptr = mmap(0, sring_sz, PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_POPULATE,
                  ring->ring_fd, IORING_OFF_SQ_RING);
    if (sq_ptr == MAP_FAILED) {
        log_error("mmap call failed");
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
            log_error("mmap call failed");
            return 1;
        }
    }

    ring->sring_tail = sq_ptr + p.sq_off.tail;
    ring->sring_mask = sq_ptr + p.sq_off.ring_mask;
    ring->sring_array = sq_ptr + p.sq_off.array;

    ring->sqes = mmap(0, p.sq_entries * sizeof(struct io_uring_sqe),
                      PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
                      ring->ring_fd, IORING_OFF_SQES);
    if (ring->sqes == MAP_FAILED) {
        log_error("mmap call failed");
        return 1;
    }

    ring->cring_head = cq_ptr + p.cq_off.head;
    ring->cring_tail = cq_ptr + p.cq_off.tail;
    ring->cring_mask = cq_ptr + p.cq_off.ring_mask;
    ring->cqes = cq_ptr + p.cq_off.cqes;

    return 0;
}

i32 os_io_uring_wait_cqe(IO_Uring *ring, IO_Uring_Completion_Entry **completion_entry) {
    u32 head;

    i32 result = os_io_uring_enter(ring->ring_fd, 0, 1, IORING_ENTER_GETEVENTS);

    head = os_io_read_barrier(ring->cring_head);

    *completion_entry = &ring->cqes[head & (*ring->cring_mask)];
    head++;

    os_io_write_barrier(ring->cring_head, head);

    return result;
}

IO_Uring_Submission_Entry *os_io_uring_get_sqe(IO_Uring *ring, u32 *tail_out) {
    u32 tail = os_io_read_barrier(ring->sring_tail);
    u32 index = tail & *ring->sring_mask;

    IO_Uring_Submission_Entry *submission_entry = &ring->sqes[index];
    ring->sring_array[index] = index;
    *tail_out = tail;

    return submission_entry;
}

void os_io_uring_prep_sqe(IO_Uring_Submission_Entry *submission_entry, u32 opcode) {
    memset(submission_entry, 0, sizeof(*submission_entry));
    submission_entry->opcode = opcode;
}
