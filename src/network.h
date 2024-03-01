#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <stdbool.h>
#include <arpa/inet.h>

#define NET_BUF_SIZE (2 + sizeof(float) * 2 + 1)

typedef struct {
  int fd;
  struct sockaddr_in addr;
} UdpSocket;

typedef enum {
  NET_CMD_CONNECT,
  NET_CMD_READY,
  NET_CMD_UPDATE_INPUT,
  NET_CMD_UPDATE_POSITION
} NetworkCmd;

typedef enum {
  GE_PADDLE_1,
  GE_PADDLE_2,
  GE_BALL
} GameEntity;

/// Creates UDP IPv4 socket and connects to the host using 
/// @returns int - socket
/// @on error returns -1
bool connect_to_host_udp(const char *host, int port, UdpSocket *out);

/// Creates a UDP Ipv4 socket, binds it to any in addr and specified port
/// @returns int - socket
/// @on error returns -1
bool create_udp_server_socket(int port, UdpSocket *out);



bool net_check_for_connection(const UdpSocket *sock, UdpSocket *out);

void net_send_cmd_wo_args(const UdpSocket *sock, NetworkCmd net_cmd);
void net_send_position(const UdpSocket *sock, GameEntity e, float x, float y);
void net_send_input(const UdpSocket *sock, int key);
bool net_recv_cmd(const UdpSocket *sock, char *buf);

#endif // !__NETWORK_H__
