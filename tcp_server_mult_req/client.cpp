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

int32_t query(int socket, const char* user_msg) {
  char wbuf[4 + MAX_MSG_SIZE]{};

  uint32_t len = strlen(user_msg);
  if (len > MAX_MSG_SIZE) {
      std::cout << "user msg too long\n";
      return -1;
  }
  memcpy(wbuf, &len, sizeof(len));
  memcpy(&wbuf[4], user_msg, len);

  std::cout.write(&wbuf[4], len);
  std::cout << "\n";

  int32_t err = write_full(socket, wbuf, 4 + len);
  if (err) {
    std::cout << "write error";
    return err;
  }

  char rbuf[4 + MAX_MSG_SIZE]{};
  errno = 0;
  err = read_full(socket, rbuf, 4);
  if (err == 1) {
    return 1;
  }
  if (err == -1) {
      std::cout << "Message length read error: " << strerror(errno) << "\n";
      return -1;
  }

  uint32_t msg_len;
  memcpy(&msg_len, rbuf, 4);

  if (msg_len > MAX_MSG_SIZE) {
    std::cout << "returned message too long\n";
    return -1;
  }

  err = read_full(socket, &rbuf[4], msg_len);
  if (err == 1) {
    std::cout << "EOF found\n";
    return 1;
  }
  if (err == -1) {
      std::cout << "Message length read error: " << strerror(errno) << "\n";
      return -1;
  }


  std::cout.write(&rbuf[4], msg_len);
  std::cout << "\n\n";

  return 0;
}

int main() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in addr{};
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(1234);
  addr.sin_family = AF_INET;

  if (connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr))) {
    die("conn");
  }

  if (query(fd, "hello?")) {
      close(fd);
  }

  if (query(fd, "yello")) {
      close(fd);
  }

  close(fd);
  return 0;
}
