#include "rane_net.h"
#include <cstddef>

rane_socket_t* rane_tcp_connect(const char* host, int port) {
  // Stub
  return NULL;
}

int rane_tcp_send(rane_socket_t* sock, const void* data, size_t len) {
  return -1;
}

int rane_tcp_recv(rane_socket_t* sock, void* buf, size_t len) {
  return -1;
}

void rane_tcp_close(rane_socket_t* sock) {
}

int rane_http_get(const char* url, char* response, size_t max_len) {
  return -1;
}
