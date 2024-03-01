#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#include "network.h"
#include "raylib.h"


static void send_all(int fd, const char *msg, size_t msg_len, int flags,
                     const struct sockaddr_in *dest) {
  ssize_t sent = 0;
  ssize_t bytes = 0;
  do {
    bytes = sendto(fd, msg + sent, msg_len - sent, flags, (struct sockaddr*)dest, sizeof(*dest));

    if (bytes < 0) {
      // TraceLog(LOG_ERROR, "Could not write to fd %d: %s\n", fd, strerror(errno));
      return;
    }

    if (bytes == 0) break;

    sent += bytes;
  } while (sent < msg_len);
}

static bool recv_all(int fd, char *buf, size_t buf_len, int flags) {
  ssize_t received = 0;
  ssize_t bytes = 0;
  struct sockaddr_in src = {0};
  socklen_t addrlen = 0;
  do {
    bytes = recvfrom(fd, buf + received, buf_len - received, flags, (struct sockaddr*)&src, &addrlen);

    if (bytes < 0) {
      // TraceLog(LOG_ERROR, "Could not read from fd %d: %s\n", fd, strerror(errno));
      return false;
    }

    if (bytes == 0) break;

    received += bytes;
  } while (received < buf_len);

  // if received == buf_len then we run out of space in buf
  return received <= buf_len;
}


bool connect_to_host_udp(const char *host, int port, UdpSocket *out) {
  struct hostent *server = NULL;
  struct sockaddr_in server_addr = {0};

  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (-1 == sock) {
    TraceLog(LOG_ERROR, "Could not create a socket: %s\n", strerror(errno));
    return false;
  }

  server = gethostbyname(host);
  if (NULL == server) {
    TraceLog(LOG_ERROR, "Could not find the host: %s\n", host);
    close(sock);
    return false;
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  memcpy(&server_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);

  if (-1 == connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
    TraceLog(LOG_ERROR, "Could not connect to the host %s: %s\n", host, strerror(errno));
    close(sock);
    return false;
  }

  out->fd = sock;
  out->addr = server_addr;
  return true;
}

bool create_udp_server_socket(int port, UdpSocket *out) {
  int server_socket;
  struct sockaddr_in server_addr = {0};

  if ((server_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    TraceLog(LOG_ERROR, "Could not create a socket: %s\n", strerror(errno));
    return false;
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
    TraceLog(LOG_ERROR, "Could not bind a socket: %s\n", strerror(errno));
    close(server_socket);
    return false;
  }

  out->fd = server_socket;
  out->addr = server_addr;
  return true;
}

bool net_check_for_connection(const UdpSocket *sock, UdpSocket *out) {
  char buf[NET_BUF_SIZE] = {0};
  struct sockaddr_in client_addr = {0};
  socklen_t addrlen = sizeof(client_addr);

  if (recvfrom(sock->fd, buf, NET_BUF_SIZE, MSG_DONTWAIT, (struct sockaddr*)&client_addr, &addrlen) != NET_BUF_SIZE
    || buf[0] != NET_CMD_CONNECT) {
    return false;
  }

  out->fd = sock->fd;
  out->addr = client_addr;
  return true;
}

void net_send_cmd_wo_args(const UdpSocket *sock, NetworkCmd net_cmd) {
  char buf[NET_BUF_SIZE] = {0};
  buf[0] = (char)net_cmd;
  send_all(sock->fd, buf, sizeof(buf), 0, &sock->addr);
}

void net_send_position(const UdpSocket *sock, GameEntity e, float x, float y) {
  // TODO: convert floats to network byte order
  char buf[2 + sizeof(float) * 2 + 1] = {0};
  char *ptr = buf;
  *ptr = (char)NET_CMD_UPDATE_POSITION;
  ptr += 1;
  *ptr = (char)e;
  ptr += 1;
  memcpy(ptr, &x, sizeof(x));
  ptr += sizeof(x);
  memcpy(ptr, &y, sizeof(y));

  send_all(sock->fd, buf, sizeof(buf), 0, &sock->addr);
}

void net_send_input(const UdpSocket *sock, int key) {
  // TODO: convert int to network byte order
  char buf[NET_BUF_SIZE] = {0};
  buf[0] = (char)NET_CMD_UPDATE_INPUT;
  memcpy(buf + 1, &key, sizeof(key));
  send_all(sock->fd, buf, sizeof(buf), 0, &sock->addr);
}

bool net_recv_cmd(const UdpSocket *sock, char *buf) {
  return recv_all(sock->fd, buf, NET_BUF_SIZE, MSG_DONTWAIT);
}
