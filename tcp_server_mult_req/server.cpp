#include <asm-generic/socket.h>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <netinet/in.h> // sockaddr_in
#include <stdarg.h>     // die()
#include <stdio.h>      // die()
#include <stdlib.h>     // die()
#include <sys/socket.h> // socket(), setsockopt()
#include <sys/socket.h>
#include <unistd.h> // close()

static int32_t read_full(int fd, char *buf, size_t size) {
  while (size > 0) {
    ssize_t n = read(fd, buf, size);

    if (n == 0)
      return 1;
    if (n < 0) {
      return -1;
    }

    assert((size_t)n <= size);

    size -= (size_t)n;
    buf += n;
  }

  return 0;
}

static int32_t write_full(int fd, char *buf, size_t size) {
  while (size > 0) {
    ssize_t n = write(fd, buf, size);

    if (n <= 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }

    assert((size_t)n <= size);

    size -= (size_t)n;
    buf += n;
  }

  return 0;
}

const uint32_t MAX_MSG_SIZE = 4096;

int action(int connfd) {
  char buf[MAX_MSG_SIZE + 4]{};

  errno = 0;
  int32_t err = read_full(connfd, buf, 4);
  if (err == 1) {
    return 1;
  }

  if (err == -1) {
    std::cout << "Message length read error: " << strerror(errno) << "\n";
    return -1;
  }

  uint32_t msg_len;
  memcpy(&msg_len, buf, 4);
  if (msg_len > MAX_MSG_SIZE) {
    std::cout << "Message too large\n";
    return -1;
  }

  err = read_full(connfd, &buf[4], msg_len);
  if (err == 1) {
    std::cout << "EOF found\n";
    return -1;
  }
  if (err == -1) {
    std::cout << "Message length read error: " << strerror(errno) << "\n";
    return -1;
  }

  std::cout.write(&buf[4], msg_len);
  std::cout << "\n";

  char wbuf[]{"yello!"};
  char write_out[4 + MAX_MSG_SIZE];

  uint32_t len = strlen(wbuf);
  memcpy(write_out, &len, 4);
  memcpy(&write_out[4], wbuf, len);

  return write_full(connfd, write_out, len + 4);
}

// Terminates the program after printing an error message
static void die(const char *format, ...) {
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
  // server = socket(),
  int fd = socket(AF_INET, SOCK_STREAM, 0);

  int val = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(1234);
  addr.sin_addr.s_addr = htonl(0);

  if (bind(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr))) {
    die("bind()");
  }

  if (listen(fd, SOMAXCONN) == -1) {
    die("listen()");
  }

  while (true) {
    sockaddr_in client{};
    socklen_t caddr_len = sizeof(client);

    int conn = accept(fd, reinterpret_cast<sockaddr *>(&client), &caddr_len);

    if (conn < 0) {
      continue;
    }

    while (true) {
      int32_t err = action(conn);
      if (err < 0) {
        break;
      }
      if (err > 0) {
        std::cout << "Connection successfully ended\n";
        break;
      }
    }

    close(conn);
  }
}
