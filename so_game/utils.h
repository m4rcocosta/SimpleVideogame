#include "clientList.h"
#include "common.h"

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

typedef struct localWorld {
  int ids[WORLDSIZE];
  int users_online;
  char has_vehicle[WORLDSIZE];
  Vehicle** vehicles;
} localWorld;

typedef struct client_args {
  localWorld* local_world;
  struct sockaddr_in server_addr_udp;
  int socket_udp;
  int socket_tcp;
} client_args;

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

//Add user
int addUser(int ids[], int size, int id2, int* position, int* users_online);

// Get ID
int get_ID(int socket);

// Get Elevation Map
Image* get_Elevation_Map(int socket);

// Get Texture Map
Image* get_Texture_Map(int socket);

// Send Vehicle Texture
int send_Vehicle_Texture(int socket, Image *texture, int id);

// Get Vehicle Texture
Image* get_Vehicle_Texture(int socket, int id);
