// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_NETWORKING_PLATFORM_HPP
#define GREM_NETWORKING_PLATFORM_HPP

#include <GREM/build_config.hpp>

#include <system_error> // std::error_code

#ifdef _WIN32

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h> // IWYU pragma: export // DWORD, AF_INET, AF_INET6, PF_INET, SOCK_DGRAM, SOCK_STREAM, SOCKET, INVALID_SOCKET, INADDR_..., IN6ADDR_..., SOL_SOCKET, SO_REUSEADDR, SOMAXCONN, FD_.., WSADATA, WSAE..., FIONBIO, struct addrinfo, struct sockaddr, struct sockaddr_storage, struct sockaddr_in, struct sockaddr_in6, struct in_addr, struct in6_addr, accept, bind, closesocket, connect, getpeername, getsockname, inet_addr, ioctlsocket, listen, recv, recvfrom, select, send, sendto, setsockopt, socket, WSACleanup, WSAGetLastError, WSAStartup, fd_set, timeval, u_long, SD_RECEIVE, SD_SEND, SD_BOTH
#include <ws2ipdef.h> // IWYU pragma: export // INET_ADDRSTRLEN, INET6_ADDRSTRLEN
#include <ws2tcpip.h> // IWYU pragma: export // EAI_..., gai_strerror, freeaddrinfo, getaddrinfo, socklen_t, inet_pton, inet_ntop

#ifndef __MINGW32__
#pragma comment(lib, "Ws2_32.lib")
#endif

#else

#include <arpa/inet.h> // IWYU pragma: export / inet_pton, inet_ntop
#include <fcntl.h>     // IWYU pragma: export // fcntl, F_GETFL, F_SETFL, O_NONBLOCK
#include <netdb.h>     // IWYU pragma: export // EAI_..., gai_strerror, struct addrinfo, freeaddrinfo, getaddrinfo
#include <netinet/in.h> // IWYU pragma: export // INADDR_..., IN6ADDR_..., struct sockaddr_in, struct sockaddr_in6, struct in_addr, struct in6_addr, INET_ADDRSTRLEN, INET6_ADDRSTRLEN
#include <poll.h>       // IWYU pragma: export // POLLIN, POLLOUT, nfds_t, pollfd, poll
#include <sys/socket.h> // IWYU pragma: export // AF_INET, AF_INET6, PF_INET, SOCK_DGRAM, SOCK_STREAM, SO_REUSEADDR, SOMAXCONN, MSG_NOSIGNAL, socklen_t, struct sockaddr, struct sockaddr_storage, accept, bind, connect, getpeername, getsockname, listen, recv, recvfrom, send, sendto, setsockopt, socket, SHUT_RD, SHUT_WR, SHUT_RDWR
#include <time.h>       // IWYU pragma: export // timeval // NOLINT(modernize-deprecated-headers)
#include <unistd.h>     // IWYU pragma: export // close

inline constexpr int SD_RECEIVE = SHUT_RD;
inline constexpr int SD_SEND = SHUT_WR;
inline constexpr int SD_BOTH = SHUT_RDWR;

using SOCKET = int;

inline constexpr int INVALID_SOCKET = -1;

inline int closesocket(SOCKET handle) {
	return close(handle);
}

#endif

namespace grem::networking {

GREM_API(networking) void ensurePlatformInitialized(std::error_code& errorCode);

} // namespace grem::networking

#endif
