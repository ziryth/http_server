
#ifndef BASE_OS_LINUX_H
#define BASE_OS_LINUX_H

#include "base_buffer.h"
#include "base_core.h"

#include "linux/io_uring.h"
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define SYS_WRITE 1
#define SYS_CLOSE 3
#define SYS_SOCKET 41
#define SYS_ACCEPT 43
#define SYS_BIND 49
#define SYS_LISTEN 50
#define SYS_EXIT 60
#define SYS_IO_URING_SETUP 425
#define SYS_IO_URING_ENTER 426
#define SYS_IO_URING_REGISTER 427

#define AF_INET 2

#define SOCK_STREAM 1

//////////////////////////////
//  Process

void os_abort(i32 exit_code);

//////////////////////////////
//  Network

typedef struct SockAddrIPv4 SockAddrIPv4;
struct SockAddrIPv4 {
    u16 family;
    u16 port;
    u32 addr;
    u8 zero[8];
};

i32 os_socket_ipv4();
i32 os_bind_ipv4(u32 sock_fd, u16 port);
i32 os_listen(u32 sock_fd, u32 backlog);
i32 os_accept(u32 sock_fd);
i32 os_write(i32 sock_fd, Buffer buffer);
i32 os_close(i32 fd);
i32 os_bind_and_listen(u32 port, u32 backlog);

//////////////////////////////
//  IO

#define IO_URING_QUEUE_DEPTH 256

typedef struct io_uring_params IO_Uring_Params;
typedef struct io_uring_sqe IO_Uring_Submission_Entry;
typedef struct io_uring_cqe IO_Uring_Completion_Entry;

typedef struct IO_Uring IO_Uring;
struct IO_Uring {
    i32 ring_fd;
    u32 *sring_tail;
    u32 *sring_mask;
    u32 *sring_array;
    u32 *cring_head;
    u32 *cring_tail;
    u32 *cring_mask;

    IO_Uring_Submission_Entry *sqes;
    IO_Uring_Completion_Entry *cqes;
};

#define os_io_read_barrier(p) __atomic_load_n((p), __ATOMIC_ACQUIRE)
#define os_io_write_barrier(p, v) __atomic_store_n((p), (v), __ATOMIC_RELEASE)

i32 os_io_uring_setup(u32 entries, IO_Uring_Params *p);
i32 os_io_uring_enter(i32 ring_fd, u32 to_submit, u32 min_complete, u32 flags);

i32 os_io_uring_init_ring(IO_Uring *ring);
i32 os_io_uring_wait_cqe(IO_Uring *ring, IO_Uring_Completion_Entry **completion_entry);
IO_Uring_Submission_Entry *os_io_uring_get_sqe(IO_Uring *ring, u32 *tail_out);
void os_io_uring_prep_sqe(IO_Uring_Submission_Entry *submission_entry, u32 opcode);

#endif // BASE_OS_LINUX_H
