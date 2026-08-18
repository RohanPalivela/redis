#include <asm-generic/socket.h>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h> // sockaddr_in
#include <poll.h>
#include <stdarg.h> // die()
#include <stdio.h>	// die()
#include <stdlib.h> // die()
#include <sys/poll.h>
#include <sys/socket.h> // socket(), setsockopt()
#include <sys/socket.h>
#include <unistd.h> // close()
#include <vector>

struct Conn {
	int fd = -1;

	bool want_read = false;
	bool want_write = false;
	bool want_close = false;

	std::vector<uint8_t> incoming;
	std::vector<uint8_t> outgoing;
};

const uint32_t MAX_MSG_SIZE = 4096;

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

static void fd_set_nb(int fd) {
	errno = 0;
	int flags = fcntl(fd, F_GETFL, 0);
	if (errno) {
		die("fcntl 1");
	}

	(void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);

	if (errno) {
		die("fcntl 2");
	}
}

static Conn *handle_accept(int fd) {
	struct sockaddr_in client_addr {};
	socklen_t addrlen = sizeof(client_addr);

	int connfd =
		accept(fd, reinterpret_cast<sockaddr *>(&client_addr), &addrlen);

	if (connfd < 0)
		return nullptr;

	fd_set_nb(connfd);

	Conn *conn = new Conn();
	conn->fd = connfd;
	conn->want_read = true; // first request is coming from client

	return conn;
}

// struct Buffer {
//     uint8_t *buf_start;
//     uint8_t *buf_end;
//     uint8_t *data_start;
//     uint8_t *data_end;
// };

// static void buf_consume(struct Buffer &buffer, size_t len) {
//     buffer.data_start += len;
// }

// static void buf_append(struct Buffer &buffer, size_t len) {
//     buffer.data_start += len;
// }

static void buf_append(std::vector<uint8_t> &buffer, const uint8_t *data,
					   size_t len) {
	buffer.insert(buffer.end(), data, data + len);
}

static void buf_consume(std::vector<uint8_t> &buffer, size_t len) {
	buffer.erase(buffer.begin(), buffer.begin() + len);
}

static bool try_one_request(Conn *conn) {
	// header
	if (conn->incoming.size() < 4) {
		return false;
	}

	uint32_t len = 0;
	memcpy(&len, conn->incoming.data(), 4);
	if (len > MAX_MSG_SIZE) {
		conn->want_close = true;
		return false;
	}

	if (4 + len > conn->incoming.size()) {
		return false;
	}

	const uint8_t *req = &conn->incoming[4];

	// processing goes here (if we cared rn)
	buf_append(conn->outgoing, (const uint8_t *)&len, 4);
	buf_append(conn->outgoing, req, len);

	buf_consume(conn->incoming, len + 4);

	return true;
}

static void handle_read(Conn *conn) {
	if (conn->outgoing.size() > 0) {
		conn->want_read = false;
		conn->want_write = true;
	}

	uint8_t buf[64 * 1024];
	ssize_t rv = read(conn->fd, buf, sizeof(buf));

	if (rv == 0) {
		conn->want_close = true;
		return;
	}

	buf_append(conn->incoming, buf, (size_t)rv);

	while (try_one_request(conn));

	if (conn->incoming.size() == 0) {
        conn->want_read = false;
        conn->want_write = true;
    }
}

static void handle_write(Conn *conn) {
    assert(conn->outgoing.size() > 0);
    ssize_t rv = write(conn->fd, conn->outgoing.data(), conn->outgoing.size());

    if (rv < 0) {
        conn->want_close = true;
        return;
    }

    buf_consume(conn->outgoing, (size_t) rv);

    if (conn->outgoing.size() == 0) {
        conn->want_read = true;
        conn->want_write = false;
    }
}

int main() {
	// server = socket(),
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	fd_set_nb(fd);

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

	std::vector<Conn *> FD_to_CONN;
	std::vector<struct pollfd> poll_args;
	while (true) {
		// sockaddr_in client{};
		// socklen_t caddr_len = sizeof(client);
		// int conn =
		// 	accept(fd, reinterpret_cast<sockaddr *>(&client), &caddr_len);

		poll_args.clear();
		struct pollfd pfd = {fd, POLLIN, 0};
		poll_args.push_back(pfd);

		for (Conn *conn : FD_to_CONN) {
			if (!conn) {
				continue;
			}

			struct pollfd pfd = {conn->fd, POLLERR, 0};

			if (conn->want_read) {
				pfd.events |= POLLIN;
			}

			if (conn->want_write) {
				pfd.events |= POLLOUT;
			}

			poll_args.push_back(pfd);
		}

		int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), -1);

		if (rv < 0 && errno == EINTR) {
			continue;
		}
		if (rv < 0) {
			die("poll");
		}

		if (poll_args[0].revents) {
			if (Conn *conn = handle_accept(fd)) {
				if (FD_to_CONN.size() <= (size_t)conn->fd) {
					FD_to_CONN.resize(conn->fd + 1);
				}
				FD_to_CONN[conn->fd] = conn;
			}
		}

		for (size_t i = 1; i < poll_args.size(); ++i) {
			uint32_t ready = poll_args[i].revents;
			Conn *conn = FD_to_CONN[poll_args[i].fd];

			if (ready & POLLIN) {
				handle_read(conn);
			}
			if (!conn->want_close && (ready & POLLOUT)) {
				handle_write(conn);
			}

			if ((ready & POLLERR) || conn->want_close) {
				close(conn->fd);
				FD_to_CONN[conn->fd] = nullptr;
				delete conn;
			}
		}
	}
}
