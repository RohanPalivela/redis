#include <asm-generic/socket.h>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <netinet/in.h> // sockaddr_in
#include <stdarg.h>		// die()
#include <stdio.h>		// die()
#include <stdlib.h>		// die()
#include <sys/socket.h> // socket(), setsockopt()
#include <sys/socket.h>
#include <unistd.h> // close()
#include <vector>

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

static void buf_append(std::vector<uint8_t> &buffer, const uint8_t *data,
					   size_t len) {
	buffer.insert(buffer.end(), data, data + len);
}

static void buf_consume(std::vector<uint8_t> &buffer, size_t len) {
	buffer.erase(buffer.begin(), buffer.begin() + len);
}

static int32_t read_full(int fd, uint8_t *buf, size_t size) {
	while (size > 0) {
		ssize_t n = read(fd, buf, size);

		if (n == 0)
			return 1;
		if (n < 0) {
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

static int32_t write_full(int fd, const uint8_t *buf, size_t size) {
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

const uint32_t MAX_MSG_SIZE = 32 << 20;

int32_t write_req(int socket, const uint8_t *user_msg, size_t length) {
   	if (length > MAX_MSG_SIZE) {
        die("write len too long");
		return -1;

    std::vector<uint8_t> wbuf;
	wbuf.reserve(length + 4);
	uint32_t msg_len = static_cast<uint32_t>(length);

	if (length > MAX_MSG_SIZE) {
		std::cout << "user msg too long\n";
		return -1;
	}

	buf_append(wbuf, (const uint8_t *)&msg_len, 4);
	buf_append(wbuf, user_msg, length);

	int32_t err = write_full(socket, wbuf.data(), wbuf.size());
	if (err) {
		std::cout << "write error";
		return err;
	}

	return 0;
}

int32_t read_req(int socket) {
	std::vector<uint8_t> rbuf;
	rbuf.resize(4);
	errno = 0;
	int err = read_full(socket, rbuf.data(), 4);
	if (err == 1) {
		return 1;
	}

	if (err == -1) {
		std::cout << "Message length read error: " << strerror(errno) << "\n";
		return -1;
	}

	uint32_t msg_len;
	memcpy(&msg_len, rbuf.data(), 4);

	if (msg_len > MAX_MSG_SIZE) {
		std::cout << "returned message too long\n";
		return -1;
	}

	rbuf.resize(4 + msg_len);
	err = read_full(socket, rbuf.data() + 4, msg_len);
	if (err == 1) {
		std::cout << "EOF found\n";
		return 1;
	}
	if (err == -1) {
		std::cout << "Message length read error: " << strerror(errno) << "\n";
		return -1;
	}

	std::cout << "server msg: ";
	std::cout.write(rbuf.data() + 4, msg_len);
	std::cout << "\n\n";

	return 0;
}

int main() {
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		die("fd init");
	}

	sockaddr_in addr{};
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(1234);
	addr.sin_family = AF_INET;

	if (connect(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr))) {
		die("conn");
	}

	std::vector<std::string> queries{"hello1", "hello2", "hello3"};

	for (const std::string &q : queries) {
		int32_t err = write_req(fd, (const uint8_t *)q.data(), q.size());
		if (err) {
			goto QUIT;
		}
	}

	for (size_t i = 0; i < queries.size(); ++i) {
		int32_t err = read_req(fd);
		if (err) {
			goto QUIT;
		}
	}

QUIT:
	close(fd);
	return 0;
}
