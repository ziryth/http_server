// #include "base/base_buffer.h"
#include "base/base_inc.h"
// #include "base/base_os_linux.h"

// int main(void) {
//
//     printf("socket\n");
//     i32 server_fd = os_socket_ipv4();
//
//     printf("server_fd: %d\n", server_fd);
//
//     printf("bind\n");
//     if (os_bind_ipv4(server_fd, 8080) == -1) {
//         printf("error binding\n");
//     }
//     printf("listen\n");
//     os_listen(server_fd, 3);
//
//     printf("accept\n");
//     i32 client_fd = os_accept(server_fd);
//     printf("client_fd: %d\n", client_fd);
//
//     printf("write\n");
//     syscall3(1, client_fd, (long)"Hello\n", 6);
//
//     return 0;
// }

i32 main(void) {
    printf("socket\n");
    i32 server_fd = os_socket_ipv4();
    if (server_fd < 0) {
        printf("socket failed: %d\n", server_fd);
        return 1;
    }
    printf("server_fd: %d\n", server_fd);

    printf("bind\n");
    i32 bind_result = os_bind_ipv4(server_fd, 8082);
    if (bind_result < 0) {
        printf("bind failed: %d\n", bind_result);
        return 1;
    }

    printf("listen\n");
    i32 listen_result = os_listen(server_fd, 3);
    if (listen_result < 0) {
        printf("listen failed: %d\n", listen_result);
        return 1;
    }

    printf("Listening on port 8080...\n");

    printf("accept\n");
    i32 client_fd = os_accept(server_fd);
    if (client_fd < 0) {
        printf("accept failed: %d\n", client_fd);
        return 1;
    }
    printf("client_fd: %d\n", client_fd);

    printf("write\n");
    Buffer output = String8("Hello, world\n");
    os_write(client_fd, output);

    return 0;
}
