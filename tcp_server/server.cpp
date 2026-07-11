#include <asm-generic/socket.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h> // sockaddr_in
#include <ostream>
#include <sys/socket.h> // socket(), setsockopt()
#include <unistd.h> // close()
#include <stdlib.h> // die()
#include <stdio.h> // die()
#include <stdarg.h> // die()

void action(int connfd) {
    char buf[64]{};
    ssize_t n = read(connfd, buf, sizeof(buf) - 1);

    if (n < 0) {
        std::cout << "read error\n";
    }
    std::cout << "Client message: " << buf << "\n";

    char wbuf[] = "yello!";
    write(connfd, wbuf, strlen(wbuf));
}

// Terminates the program after printing an error message
void die(const char *format, ...) {
    va_list args;
    va_start(args, format);

    // Print the error message to stderr
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");

    va_end(args);

    // Terminate the process
    exit(EXIT_FAILURE);
}

int main() {
    /*
     * AF_INET = IPv4
     * SOCK_STREAM = TCP (DGRAM = UDP)
     */
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    int val = 1;
    /*
     * Parameters:
     * int __fd = fd socket (tcp ipv4 connection before)
     * int __level =      \
     * int __optname = SO_REUSEADDR = 1 : / <- socket behavior configs
     * const void * __optval
     * socklen_t __optlen (aka unsigned int)
     */
    /*
     * Note from book:
     * > The effect of SO_REUSEADDR is important: if it’s not set to 1, a server program cannot bind to the same IP:port it was using after a restart. This is generally undesirable TCP behavior. You should enable SO_REUSEADDR for all listening sockets! Even if you don’t understand what exactly it is.
     */
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(0);

    int rv = bind(fd, (const sockaddr *)&addr, sizeof(addr));
    if (rv) { die("bind()"); }

    rv = listen(fd, SOMAXCONN);
    if (rv) { die("listen()"); }

    while (true) {
        sockaddr_in client_addr = {};
        socklen_t addrlen = sizeof(client_addr);

        int connfd = accept(fd, (sockaddr *) &client_addr, &addrlen);

        if (connfd < 0) {
            continue; // error condition
        }

        action(connfd);
        close(connfd);
    }
}
