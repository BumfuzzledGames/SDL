#if defined _WIN32
#	ifndef _WIN32_WINNT
#		define _WIN32_WINNT 0x501
#	endif
#	define WIN32_LEAN_AND_MEAN
#	include <windows.h>
#	include <winsock2.h>
#	include <ws2tcpip.h>
#	pragma comment (lib, "Ws2_32.lib")
#else
//	Assume we're on a POSIX system if it's not Windows
#	include <sys/socket.h>
#	include <arpa/inet.h>
#	include <netdb.h>
#	include <unistd.h>
#endif
#include <stdio.h>
#include "SDL_log.h"
#include "SDL_stdinc.h"
#include "SDL_thread.h"
#include "fakekb.h"

#if !defined _WIN32
typedef int SOCKET;
#define INVALID_SOCKET -1
#endif

#define DEFAULT_PORT "6502"

static int initialized = 0;
static SOCKET listen_socket = INVALID_SOCKET;
static SDL_mutex* mutex = NULL;
static SDL_Thread* server_thread = NULL;


static int sock_init() {
#if defined _WIN32
	WSADATA was_data;
	return WSAStartup(MAKEWORD(1, 1), &was_data);
#else
	return 0;
#endif
}


static int sock_quit() {
#if defined _WIN32
	return WSACleanup();
#else
	return 0;
#endif
}


static SOCKET sock_open() {
	return 0;
}


static int sock_close(SOCKET s) {
	int status;

#if defined _WIN32
	status = shutdown(s, SD_BOTH);
	if (status == 0) {
		status = closesocket(s);
	}
#else
	status = shutdown(s, SHUT_RDWR);
	if (state == 0) {
		status = close(s);
	}
#endif

	return status;
}


static int sock_error() {
#	if defined _WIN32
	return WSAGetLastError();
#	else
	return errno;
#	endif
}


static char* sock_error_string(int error) {
#	if defined _WIN32
#	define BUFFER_SIZE 2048
	static char buffer[BUFFER_SIZE] = { 0 };
	FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		error,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		buffer,
		BUFFER_SIZE,
		NULL
	);
	return &buffer;
#	undef BUFFER_SIZE
#	else
	return strerror(error);
#	endif
}


void handle_client(char* buffer) {
}


static int server_func(void* data) {
#	define BUFFER_SIZE 2048
#	define FAIL(msg, ...) do { SDL_LogError(0, msg, __VA_ARGS__); goto done; } while(0)
	struct fd_set master_set;
	struct fd_set working_set;
	SOCKET max_sock;

	static char buffer[BUFFER_SIZE] = { 0 };

	FD_ZERO(&master_set);
	max_sock = listen_socket;
	FD_SET(listen_socket, &master_set);

	for (;;) {
		memcpy(&working_set, &master_set, sizeof(master_set));
		int sockets_ready = select(max_sock + 1, &working_set, NULL, NULL, NULL);
		if (sockets_ready < 0) {
			FAIL("select failed");
		}
		for (SOCKET s = 0; s <= max_sock && sockets_ready > 0; s++) {
			if (FD_ISSET(s, &working_set)) {
				sockets_ready--;
				if (s == listen_socket) {
					// Server socket
					SOCKET new_client = accept(listen_socket, NULL, NULL);
					if (new_client == INVALID_SOCKET) FAIL("accept failed");
					FD_SET(new_client, &master_set);
					if (new_client > max_sock) max_sock = new_client;
				}
				else {
					// Client socket
					int should_close = 0;
					int return_code = recv(s, buffer, sizeof(buffer), 0);
					if (return_code < 0) {
						if (errno != EWOULDBLOCK) {
							should_close = 1;
						}
					}
					else if (return_code == 0) {
						should_close = 1;
					}
					else {
						handle_client(buffer);
					}
					if (should_close) {
						sock_close(s);
						FD_CLR(s, &master_set);
						if (s == max_sock) {
							while (!FD_ISSET(max_sock, &master_set)) max_sock--;
						}
					}
				}
			}
		}
	}

done:
	return 0;
#	undef BUFFER_SIZE
#	undef FAIL
}


int fakekb_init() {
#	define FAIL(fmt, ...) do { SDL_LogError(0, fmt, __VA_ARGS__); goto fail; } while(0)
	struct addrinfo* result = NULL;

	if (!initialized) {

		if (sock_init() != 0) FAIL("sock_init failed");

		struct addrinfo hints = {
			.ai_family = AF_INET,
			.ai_socktype = SOCK_STREAM,
			.ai_protocol = IPPROTO_TCP,
			.ai_flags = AI_PASSIVE
		};

		if (getaddrinfo(NULL, DEFAULT_PORT, &hints, &result) != 0) FAIL("getaddr_info failed");

		listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if (listen_socket == INVALID_SOCKET) FAIL("socket failed");
		int opt = 0;
		if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) < 0) FAIL("setsockopt failed");

		if (bind(listen_socket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) FAIL("bind failed: %s", sock_error_string(sock_error()));

		freeaddrinfo(result);
		result = NULL;

		if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) FAIL("listen failed");

		mutex = SDL_CreateMutex();
		if (mutex == NULL) FAIL("SDL_CreateMutex failed");

		server_thread = SDL_CreateThread(server_func, "fakekb Server Thread", NULL);
		if (server_thread == NULL) FAIL("SDL_CreateThread failed");

		initialized = 1;
		SDL_LogInfo(0, "fakekb initialized");
	}

	return 0;

fail:
	if (result != NULL) {
		freeaddrinfo(result);
	}
	if (listen_socket != INVALID_SOCKET) {
		sock_close(listen_socket);
		listen_socket = INVALID_SOCKET;
	}
	if (mutex != NULL) {
		SDL_DestroyMutex(mutex);
		mutex = NULL;
	}
	if (server_thread != NULL) {
		// The server thread should finish when we closed listen_socket
		SDL_WaitThread(server_thread, NULL);
		server_thread = NULL;
	}
	sock_quit();
	return -1;
#	undef FAIL
}


void fakekb_quit() {
	if (initialized) {
		if (mutex != NULL) {
			SDL_DestroyMutex(mutex);
			mutex = NULL;
		}
		if (listen_socket != INVALID_SOCKET) {
			sock_close(listen_socket);
			listen_socket = INVALID_SOCKET;
		}
		sock_quit();
		initialized = 0;
	}
}