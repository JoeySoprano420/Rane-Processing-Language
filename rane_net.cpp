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
    rane_net_set_last_error("getaddrinfo failed");
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

// -----------------------------
// HTTP/1.1 GET implementation
// -----------------------------

static int ascii_tolower(int c) {
  if (c >= 'A' && c <= 'Z') return c + 32;
  return c;
}

static int strncaseeq(const char* a, const char* b, size_t n) {
  for (size_t i = 0; i < n; i++) {
    char ca = a[i];
    char cb = b[i];
    if (ca == 0 || cb == 0) return 0;
    if (ascii_tolower((unsigned char)ca) != ascii_tolower((unsigned char)cb)) return 0;
  }
  return 1;
}

static const char* find_header_end(const char* buf, size_t len) {
  if (!buf || len < 4) return NULL;
  for (size_t i = 0; i + 3 < len; i++) {
    if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') {
      return buf + i + 4;
    }
  }
  return NULL;
}

static int parse_url_http(const char* url, char* host, size_t host_cap, int* out_port, char* path, size_t path_cap) {
  if (!url || !host || !out_port || !path) return 0;
  if (host_cap == 0 || path_cap == 0) return 0;

  host[0] = 0;
  path[0] = 0;
  *out_port = 80;

  const char* p = url;
  const char* prefix = "http://";
  if (!strncaseeq(p, prefix, 7)) {
    rane_net_set_last_error("only http:// URLs supported");
    return 0;
  }
  p += 7;

  const char* host_begin = p;
  while (*p && *p != '/' && *p != ':') p++;
  const char* host_end = p;

  size_t host_len = (size_t)(host_end - host_begin);
  if (host_len == 0 || host_len + 1 > host_cap) {
    rane_net_set_last_error("host too long");
    return 0;
  }
#if defined(_WIN32)
  strncpy_s(host, host_cap, host_begin, host_len);
#else
  memcpy(host, host_begin, host_len);
  host[host_len] = 0;
#endif

  // Optional port
  if (*p == ':') {
    p++;
    int port = 0;
    int digits = 0;
    while (*p && *p != '/') {
      if (*p < '0' || *p > '9') {
        rane_net_set_last_error("invalid port");
        return 0;
      }
      port = port * 10 + (*p - '0');
      if (port > 65535) {
        rane_net_set_last_error("port out of range");
        return 0;
      }
      p++;
      digits++;
    }
    if (digits == 0) {
      rane_net_set_last_error("empty port");
      return 0;
    }
    *out_port = port;
  }

  // Path
  if (*p == 0) {
#if defined(_WIN32)
    strncpy_s(path, path_cap, "/", _TRUNCATE);
#else
    strncpy(path, "/", path_cap - 1);
    path[path_cap - 1] = 0;
#endif
    return 1;
  }

  if (*p != '/') {
    rane_net_set_last_error("invalid URL path");
    return 0;
  }

  size_t path_len = strlen(p);
  if (path_len + 1 > path_cap) {
    rane_net_set_last_error("path too long");
    return 0;
  }
#if defined(_WIN32)
  strncpy_s(path, path_cap, p, _TRUNCATE);
#else
  strncpy(path, p, path_cap - 1);
  path[path_cap - 1] = 0;
#endif

  return 1;
}

static int send_all(rane_socket_t* s, const void* data, size_t len) {
  const uint8_t* p = (const uint8_t*)data;
  size_t off = 0;
  while (off < len) {
    int n = rane_tcp_send(s, p + off, len - off);
    if (n <= 0) return 0;
    off += (size_t)n;
  }
  return 1;
}

static int recv_some(rane_socket_t* s, uint8_t* buf, size_t cap, size_t* out_n) {
  if (out_n) *out_n = 0;
  if (!s || !buf || cap == 0 || !out_n) return 0;
  int n = rane_tcp_recv(s, buf, cap);
  if (n < 0) return 0;
  *out_n = (size_t)n;
  return 1;
}

static int parse_status_code(const char* hdr, size_t hdr_len, int* out_code) {
  if (out_code) *out_code = 0;
  if (!hdr || hdr_len < 12 || !out_code) return 0;

  // "HTTP/1.1 200 ..."
  const char* sp = (const char*)memchr(hdr, ' ', hdr_len);
  if (!sp) return 0;
  if ((size_t)(sp - hdr + 4) > hdr_len) return 0;
  int code = 0;
  if (sp[1] < '0' || sp[1] > '9') return 0;
  if (sp[2] < '0' || sp[2] > '9') return 0;
  if (sp[3] < '0' || sp[3] > '9') return 0;
  code = (sp[1] - '0') * 100 + (sp[2] - '0') * 10 + (sp[3] - '0');
  *out_code = code;
  return 1;
}

static int header_find_token(const char* headers, size_t headers_len, const char* key, const char** out_val, size_t* out_val_len) {
  if (out_val) *out_val = NULL;
  if (out_val_len) *out_val_len = 0;
  if (!headers || !key || !out_val || !out_val_len) return 0;

  size_t key_len = strlen(key);
  const char* p = headers;
  const char* end = headers + headers_len;

  while (p < end) {
    const char* line_end = (const char*)memchr(p, '\n', (size_t)(end - p));
    if (!line_end) line_end = end;
    const char* line = p;

    // advance to next line for next iter
    p = (line_end < end) ? (line_end + 1) : end;

    // skip possible \r
    const char* line_real_end = line_end;
    if (line_real_end > line && line_real_end[-1] == '\r') line_real_end--;

    // empty line => end headers
    if (line_real_end == line) break;

    const char* colon = (const char*)memchr(line, ':', (size_t)(line_real_end - line));
    if (!colon) continue;

    size_t name_len = (size_t)(colon - line);
    if (name_len != key_len) continue;

    if (!strncaseeq(line, key, key_len)) continue;

    const char* v = colon + 1;
    while (v < line_real_end && (*v == ' ' || *v == '\t')) v++;
    *out_val = v;
    *out_val_len = (size_t)(line_real_end - v);
    return 1;
  }

  return 0;
}

static int parse_uint_dec(const char* s, size_t len, uint64_t* out) {
  if (out) *out = 0;
  if (!s || !out || len == 0) return 0;
  uint64_t v = 0;
  for (size_t i = 0; i < len; i++) {
    char c = s[i];
    if (c < '0' || c > '9') return 0;
    uint64_t nv = v * 10 + (uint64_t)(c - '0');
    if (nv < v) return 0;
    v = nv;
  }
  *out = v;
  return 1;
}

static int parse_hex_uint(const char* s, size_t len, uint64_t* out) {
  if (out) *out = 0;
  if (!s || !out || len == 0) return 0;
  uint64_t v = 0;
  for (size_t i = 0; i < len; i++) {
    char c = s[i];
    uint64_t d = 0;
    if (c >= '0' && c <= '9') d = (uint64_t)(c - '0');
    else if (c >= 'a' && c <= 'f') d = (uint64_t)(10 + c - 'a');
    else if (c >= 'A' && c <= 'F') d = (uint64_t)(10 + c - 'A');
    else return 0;

    uint64_t nv = (v << 4) | d;
    if (nv < v) return 0;
    v = nv;
  }
  *out = v;
  return 1;
}

static size_t trim_ws_len(const char* s, size_t len) {
  while (len && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r' || s[len - 1] == '\n')) len--;
  return len;
}

int rane_http_get(const char* url, char* response, size_t max_len) {
  if (!url || !response || max_len == 0) {
    rane_net_set_last_error("invalid arg");
    return -1;
  }

  response[0] = 0;

  char host[256];
  char path[1024];
  int port = 80;
  if (!parse_url_http(url, host, sizeof(host), &port, path, sizeof(path))) {
    return -1;
  }

  rane_socket_t* s = rane_tcp_connect(host, port);
  if (!s) return -1;

  // Minimal HTTP/1.1 request; force Connection: close (easier deterministic EOF framing)
  char req[2048];
#if defined(_WIN32)
  sprintf_s(req, sizeof(req),
            "GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "User-Agent: rane-net/1.0\r\n"
            "Accept: */*\r\n"
            "Connection: close\r\n"
            "\r\n",
            path, host);
#else
  snprintf(req, sizeof(req),
           "GET %s HTTP/1.1\r\n"
           "Host: %s\r\n"
           "User-Agent: rane-net/1.0\r\n"
           "Accept: */*\r\n"
           "Connection: close\r\n"
           "\r\n",
           path, host);
#endif

  if (!send_all(s, req, strlen(req))) {
    rane_tcp_close(s);
    return -1;
  }

  // Read response into a temporary buffer first (headers must be parsed).
  // Keep bounded for determinism; if too large, truncate with an error.
  const size_t tmp_cap = 1u << 20; // 1MB internal cap
  uint8_t* tmp = (uint8_t*)malloc(tmp_cap);
  if (!tmp) {
    rane_net_set_last_error("out of memory");
    rane_tcp_close(s);
    return -1;
  }

  size_t tmp_len = 0;
  int headers_done = 0;
  size_t header_end_off = 0;

  while (tmp_len + 4096 < tmp_cap) {
    size_t rd = 0;
    if (!recv_some(s, tmp + tmp_len, 4096, &rd)) {
      free(tmp);
      rane_tcp_close(s);
      return -1;
    }
    if (rd == 0) break;
    tmp_len += rd;

    const char* he = find_header_end((const char*)tmp, tmp_len);
    if (he) {
      headers_done = 1;
      header_end_off = (size_t)(he - (const char*)tmp);
      break;
    }
  }

  if (!headers_done) {
    rane_net_set_last_error("http: headers too large or incomplete");
    free(tmp);
    rane_tcp_close(s);
    return -1;
  }

  int status = 0;
  if (!parse_status_code((const char*)tmp, header_end_off, &status)) {
    rane_net_set_last_error("http: failed to parse status line");
    free(tmp);
    rane_tcp_close(s);
    return -1;
  }

  const char* headers = (const char*)tmp;
  size_t headers_len = header_end_off;

  // Determine transfer framing.
  const char* te = NULL;
  size_t te_len = 0;
  const char* cl = NULL;
  size_t cl_len = 0;

  int have_te = header_find_token(headers, headers_len, "Transfer-Encoding", &te, &te_len);
  int have_cl = header_find_token(headers, headers_len, "Content-Length", &cl, &cl_len);

  int chunked = 0;
  if (have_te) {
    te_len = trim_ws_len(te, te_len);
    if (te_len >= 7 && strncaseeq(te, "chunked", 7)) chunked = 1;
  }

  uint64_t content_len = 0;
  if (!chunked && have_cl) {
    cl_len = trim_ws_len(cl, cl_len);
    if (!parse_uint_dec(cl, cl_len, &content_len)) {
      rane_net_set_last_error("http: invalid Content-Length");
      free(tmp);
      rane_tcp_close(s);
      return -1;
    }
  }

  // Prepare output buffer
  size_t out_w = 0;
  size_t body_off = header_end_off;

  auto out_put = [&](const uint8_t* p, size_t n) -> int {
    if (n == 0) return 1;
    if (out_w >= max_len) return 0;
    size_t cap = max_len - out_w;
    size_t take = (n < cap) ? n : cap;
    memcpy(response + out_w, p, take);
    out_w += take;
    if (out_w < max_len) response[out_w] = 0;
    return (take == n) ? 1 : 0;
  };

  // First, consume already-read body bytes in tmp.
  if (tmp_len > body_off) {
    size_t have = tmp_len - body_off;
    if (!chunked) {
      // If content-length known, only take up to it.
      if (have_cl && content_len < have) have = (size_t)content_len;
    }
    (void)out_put(tmp + body_off, have);
  }

  // Now stream the remainder.
  if (chunked) {
    // We already copied raw body bytes; but chunked requires parsing, so we should parse from scratch.
    // For simplicity, re-parse chunked using a small state machine over socket stream.

    // Reset output and parse starting from body bytes already in tmp.
    out_w = 0;
    response[0] = 0;

    // We'll use a small rolling buffer for chunk parsing:
    // accumulate line for chunk size, then read that many bytes, then consume CRLF.
    size_t cursor = body_off;

    auto need_more = [&](size_t need) -> int {
      while ((tmp_len - cursor) < need) {
        if (tmp_len + 4096 >= tmp_cap) {
          rane_net_set_last_error("http: chunked body too large");
          return 0;
        }
        size_t rd = 0;
        if (!recv_some(s, tmp + tmp_len, 4096, &rd)) return 0;
        if (rd == 0) break;
        tmp_len += rd;
      }
      return (tmp_len - cursor) >= need;
    };

    for (;;) {
      // Read chunk size line
      const char* line = (const char*)tmp + cursor;
      const char* end = (const char*)tmp + tmp_len;
      const char* nl = NULL;

      for (const char* it = line; it < end; it++) {
        if (*it == '\n') { nl = it; break; }
      }

      while (!nl) {
        if (!need_more(1)) { rane_net_set_last_error("http: chunk size line incomplete"); free(tmp); rane_tcp_close(s); return -1; }
        end = (const char*)tmp + tmp_len;
        for (const char* it = line; it < end; it++) {
          if (*it == '\n') { nl = it; break; }
        }
      }

      const char* line_end = nl;
      // Trim CRLF
      const char* line_real_end = line_end;
      if (line_real_end > line && line_real_end[-1] == '\r') line_real_end--;

      size_t hex_len = (size_t)(line_real_end - line);
      // Ignore chunk extensions: "HEX;ext"
      for (size_t i = 0; i < hex_len; i++) {
        if (line[i] == ';') { hex_len = i; break; }
      }

      uint64_t chunk_sz = 0;
      if (!parse_hex_uint(line, hex_len, &chunk_sz)) {
        rane_net_set_last_error("http: invalid chunk size");
        free(tmp);
        rane_tcp_close(s);
        return -1;
      }

      cursor = (size_t)((nl + 1) - (const char*)tmp); // past LF

      if (chunk_sz == 0) {
        // Final chunk; consume optional trailer headers until CRLF CRLF (or just stop on close).
        // We'll best-effort: find "\r\n\r\n" starting from cursor; if not present, ignore.
        free(tmp);
        rane_tcp_close(s);
        if (out_w < max_len) response[out_w] = 0;
        return status;
      }

      // Need chunk data + CRLF
      if (chunk_sz > (uint64_t)SIZE_MAX) {
        rane_net_set_last_error("http: chunk size too large");
        free(tmp);
        rane_tcp_close(s);
        return -1;
      }

      size_t need = (size_t)chunk_sz + 2; // data + CRLF
      if (!need_more(need)) {
        rane_net_set_last_error("http: chunk data incomplete");
        free(tmp);
        rane_tcp_close(s);
        return -1;
      }

      if (!out_put(tmp + cursor, (size_t)chunk_sz)) {
        rane_net_set_last_error("http: response truncated");
        free(tmp);
        rane_tcp_close(s);
        return -1;
      }

      cursor += (size_t)chunk_sz;

      // Consume CRLF after chunk
      if (tmp[cursor] == '\r') cursor++;
      if (cursor < tmp_len && tmp[cursor] == '\n') cursor++;
    }
  } else if (have_cl) {
    // Content-Length: read exactly remaining bytes (or until output full)
    size_t already = 0;
    if (tmp_len > body_off) already = tmp_len - body_off;
    if ((uint64_t)already > content_len) already = (size_t)content_len;

    uint64_t remain64 = (content_len > (uint64_t)already) ? (content_len - (uint64_t)already) : 0;

    uint8_t buf[4096];
    while (remain64 > 0) {
      size_t want = (remain64 > sizeof(buf)) ? sizeof(buf) : (size_t)remain64;
      int n = rane_tcp_recv(s, buf, want);
      if (n < 0) { free(tmp); rane_tcp_close(s); return -1; }
      if (n == 0) break;
      remain64 -= (uint64_t)n;

      if (!out_put(buf, (size_t)n)) {
        rane_net_set_last_error("http: response truncated");
        free(tmp);
        rane_tcp_close(s);
        return -1;
      }
    }

    free(tmp);
    rane_tcp_close(s);
    if (out_w < max_len) response[out_w] = 0;
    return status;
  } else {
    // No framing headers: read until close (Connection: close enforced)
    uint8_t buf[4096];
    for (;;) {
      int n = rane_tcp_recv(s, buf, sizeof(buf));
      if (n < 0) { free(tmp); rane_tcp_close(s); return -1; }
      if (n == 0) break;

      if (!out_put(buf, (size_t)n)) {
        rane_net_set_last_error("http: response truncated");
        free(tmp);
        rane_tcp_close(s);
        return -1;
      }
    }

    free(tmp);
    rane_tcp_close(s);
    if (out_w < max_len) response[out_w] = 0;
    return status;
  }
}
