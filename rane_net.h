#pragma once

#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// Networking Library for RANE
// TCP/UDP sockets, HTTP client/server stubs

typedef struct rane_socket_s rane_socket_t;

rane_socket_t* rane_tcp_connect(const char* host, int port);
int rane_tcp_send(rane_socket_t* sock, const void* data, size_t len);
int rane_tcp_recv(rane_socket_t* sock, void* buf, size_t len);
void rane_tcp_close(rane_socket_t* sock);

// Error reporting
const char* rane_net_last_error();

// HTTP stubs
int rane_http_get(const char* url, char* response, size_t max_len);

#ifdef __cplusplus
} // extern "C"
#endif
