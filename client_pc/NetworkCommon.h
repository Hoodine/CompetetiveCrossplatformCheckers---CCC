#pragma once

#ifdef _WIN32
// Windows
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

typedef SOCKET SocketType;
#define INVALID_SOCKET_VAL INVALID_SOCKET
#define SOCKET_ERROR_VAL SOCKET_ERROR

inline int closeSocket(SocketType sock) { return closesocket(sock); }
inline int getSocketError() { return WSAGetLastError(); }
inline void initNetworking() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}
inline void cleanupNetworking() { WSACleanup(); }

#else
// Linux / POSIX
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

typedef int SocketType;
#define INVALID_SOCKET_VAL (-1)
#define SOCKET_ERROR_VAL (-1)

inline int closeSocket(SocketType sock) { return close(sock); }
inline int getSocketError() { return errno; }
inline void initNetworking() { /* ничего не нужно */ }
inline void cleanupNetworking() { /* ничего не нужно */ }

#endif