#include "base_os_linux.h"
#include "base_core.h"

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
