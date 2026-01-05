#include "rane_net.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

struct rane_socket_s {
#if defined(_WIN32)
  SOCKET s;
#else
  int s;
#endif
};

static thread_local char g_rane_net_last_err[512] = {0};

static void rane_net_set_last_error(const char* msg) {
  if (!msg) msg = "unknown error";
#if defined(_WIN32)
  strncpy_s(g_rane_net_last_err, sizeof(g_rane_net_last_err), msg, _TRUNCATE);
#else
  strncpy(g_rane_net_last_err, msg, sizeof(g_rane_net_last_err) - 1);
  g_rane_net_last_err[sizeof(g_rane_net_last_err) - 1] = 0;
#endif
}

#if defined(_WIN32)
static void rane_net_set_last_error_wsa(const char* prefix, int err) {
  char* sysmsg = NULL;
  DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD n = FormatMessageA(flags, NULL, (DWORD)err, 0, (LPSTR)&sysmsg, 0, NULL);
  if (n == 0 || !sysmsg) {
    char buf[256];
    sprintf_s(buf, sizeof(buf), "%s (wsa=%d)", prefix ? prefix : "winsock", err);
    rane_net_set_last_error(buf);
    return;
  }

  while (n > 0 && (sysmsg[n - 1] == '\r' || sysmsg[n - 1] == '\n')) sysmsg[--n] = 0;

  char buf[512];
  if (prefix) sprintf_s(buf, sizeof(buf), "%s: %s", prefix, sysmsg);
  else sprintf_s(buf, sizeof(buf), "%s", sysmsg);
  rane_net_set_last_error(buf);
  LocalFree(sysmsg);
}
#else
static void rane_net_set_last_error_errno(const char* prefix, int err) {
  const char* em = strerror(err);
  if (!em) em = "unknown errno";
  char buf[512];
  if (prefix) snprintf(buf, sizeof(buf), "%s: %s", prefix, em);
  else snprintf(buf, sizeof(buf), "%s", em);
  rane_net_set_last_error(buf);
}
#endif

const char* rane_net_last_error() {
  return g_rane_net_last_err;
}

#if defined(_WIN32)
static int g_wsa_inited = 0;
static int ensure_wsa() {
  if (g_wsa_inited) return 1;
  WSADATA w;
  int r = WSAStartup(MAKEWORD(2, 2), &w);
  if (r != 0) {
    rane_net_set_last_error_wsa("WSAStartup", r);
    return 0;
  }
  g_wsa_inited = 1;
  return 1;
}
#endif

rane_socket_t* rane_tcp_connect(const char* host, int port) {
  if (!host || port <= 0 || port > 65535) {
    rane_net_set_last_error("invalid arg");
    return NULL;
  }

#if defined(_WIN32)
  if (!ensure_wsa()) return NULL;
#endif

  char port_str[16];
#if defined(_WIN32)
  sprintf_s(port_str, sizeof(port_str), "%d", port);
#else
  snprintf(port_str, sizeof(port_str), "%d", port);
#endif

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  struct addrinfo* res = NULL;
  int gai = getaddrinfo(host, port_str, &hints, &res);
  if (gai != 0 || !res) {
#if defined(_WIN32)
    rane_net_set_last_error("getaddrinfo failed");
#else
    rane_net_set_last_error("getaddrinfo failed");
#endif
    return NULL;
  }

  rane_socket_t* out = NULL;

  for (struct addrinfo* it = res; it; it = it->ai_next) {
#if defined(_WIN32)
    SOCKET s = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
    if (s == INVALID_SOCKET) {
      rane_net_set_last_error_wsa("socket", WSAGetLastError());
      continue;
    }

    if (connect(s, it->ai_addr, (int)it->ai_addrlen) == SOCKET_ERROR) {
      rane_net_set_last_error_wsa("connect", WSAGetLastError());
      closesocket(s);
      continue;
    }

    out = (rane_socket_t*)malloc(sizeof(rane_socket_t));
    if (!out) {
      rane_net_set_last_error("out of memory");
      closesocket(s);
      break;
    }
    out->s = s;
    break;
#else
    int s = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
    if (s < 0) {
      rane_net_set_last_error_errno("socket", errno);
      continue;
    }

    if (connect(s, it->ai_addr, it->ai_addrlen) != 0) {
      rane_net_set_last_error_errno("connect", errno);
      close(s);
      continue;
    }

    out = (rane_socket_t*)malloc(sizeof(rane_socket_t));
    if (!out) {
      rane_net_set_last_error("out of memory");
      close(s);
      break;
    }
    out->s = s;
    break;
#endif
  }

  freeaddrinfo(res);
  return out;
}

int rane_tcp_send(rane_socket_t* sock, const void* data, size_t len) {
  if (!sock || !data) {
    rane_net_set_last_error("invalid arg");
    return -1;
  }
  if (len == 0) return 0;

#if defined(_WIN32)
  int sent = send(sock->s, (const char*)data, (int)len, 0);
  if (sent == SOCKET_ERROR) {
    rane_net_set_last_error_wsa("send", WSAGetLastError());
    return -1;
  }
  return sent;
#else
  ssize_t sent = send(sock->s, data, len, 0);
  if (sent < 0) {
    rane_net_set_last_error_errno("send", errno);
    return -1;
  }
  return (int)sent;
#endif
}

int rane_tcp_recv(rane_socket_t* sock, void* buf, size_t len) {
  if (!sock || !buf) {
    rane_net_set_last_error("invalid arg");
    return -1;
  }
  if (len == 0) return 0;

#if defined(_WIN32)
  int recvd = recv(sock->s, (char*)buf, (int)len, 0);
  if (recvd == SOCKET_ERROR) {
    rane_net_set_last_error_wsa("recv", WSAGetLastError());
    return -1;
  }
  return recvd;
#else
  ssize_t recvd = recv(sock->s, buf, len, 0);
  if (recvd < 0) {
    rane_net_set_last_error_errno("recv", errno);
    return -1;
  }
  return (int)recvd;
#endif
}

void rane_tcp_close(rane_socket_t* sock) {
  if (!sock) return;
#if defined(_WIN32)
  if (sock->s != INVALID_SOCKET) closesocket(sock->s);
  sock->s = INVALID_SOCKET;
#else
  if (sock->s >= 0) close(sock->s);
  sock->s = -1;
#endif
  free(sock);
}

int rane_http_get(const char* url, char* response, size_t max_len) {
  (void)url;
  (void)response;
  (void)max_len;
  rane_net_set_last_error("http not implemented");
  return -1;
}
