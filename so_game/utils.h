#include "clientList.h"

typedef struct tcp_args{
	int client_desc;
	Image* elevation_texture;
	Image* surface_elevation;
} tcp_args;

typedef struct udp_args{
    int socket_udp;
    Image* surface_texture;
    Image* surface_elevation;
    Image* vehicle_texture;
}udp_args;

typedef struct client_args {
  localWorld* local_world;
  struct sockaddr_in server_addr_udp;
  int socket_udp;
  int socket_tcp;
} client_args;

typedef struct localWorld {
  int ids[WORLDSIZE];
  int users_online;
  char has_vehicle[WORLDSIZE];
  Vehicle** vehicles;
} localWorld;

//TCP receive function
int receive_tcp(int socket, void *buffer, size_t length, int flags);

//TCP send function
int send_tcp(int socket, const void *buffer, size_t length, int flags);

//UDP receive function
int receive_udp(int socket, void *buffer, size_t length, int flags, struct sockaddr *src_addr, socklen_t *addrlen);

//UDP send function
int send_udp(int socket, const void *buffer, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);

//Has user
int hasUser(int ids[], int size, int id);
