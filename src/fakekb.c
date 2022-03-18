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
#include <stdarg.h>
#include <stdio.h>
#include "fakekb.h"

#if !defined _WIN32
typedef int SOCKET;
#define INVALID_SOCKET -1
#endif

#define DEFAULT_PORT "6502"

int initialized = 0;
SOCKET listen_socket = INVALID_SOCKET;


#if 0
void errorf(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
#	if defined _WIN32
#	define BUFFER_LEN 2048
	static char buffer[BUFFER_LEN];
	vsnprintf(buffer, BUFFER_LEN, fmt, args);
	OutputDebugString(buffer);
#	undef BUFFER_LEN
#	else
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
#	endif
	va_end(args);
}
#endif

void errorf(const char* message) {
#	if defined _WIN32
	OutputDebugString(message);
#	else
	fprintf(stderr, "%s\n", message);
#	endif
}


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


SOCKET sock_open() {
	return 0;
}


int sock_close(SOCKET s) {
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


int fakekb_init() {
#	define FAIL(fmt, ...) do { errorf(fmt, __VA_ARGS__); goto fail; } while(0)
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

		if (bind(listen_socket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) FAIL("bind failed");

		freeaddrinfo(result);
		result = NULL;

		if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) FAIL("listen failed");
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
	sock_quit();
	return -1;
#	undef FAIL
}


void fakekb_quit() {
	if (initialized) {
		if (listen_socket != INVALID_SOCKET) {
			sock_close(listen_socket);
			listen_socket = INVALID_SOCKET;
		}
		sock_quit();
		initialized = 0;
	}
}