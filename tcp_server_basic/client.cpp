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
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    // int val = 1;
    // setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    sockaddr_in addr{};
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(1234);
    addr.sin_family = AF_INET;

    int rv = connect(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr));

    if (rv) {
        die("connect");
    }

    const char msg[] = "hi!!!!!!";
    write(fd, msg, strlen(msg));

    char rbuf[64]{};
    ssize_t return_size = read(fd, rbuf, sizeof(rbuf) - 1);

    if (return_size < 0) {
        die("read");
    }

    std::cout << "server response: " << rbuf << "\n";
    close(fd);
}
