#include "clientList.h"
#include "common.h"
#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"
#include "so_game_protocol.h"

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

//Has user
int hasUser(int ids[], int size, int id);

//Add user
int addUser(int ids[], int size, int id2, int* position, int* users_online);

// Get ID
int get_ID(int socket);

// Get Elevation Map
Image* get_Elevation_Map(int socket, int id);

// Get Texture Map
Image* get_Texture_Map(int socket, int id);

// Send Vehicle Texture
int send_Vehicle_Texture(int socket, Image *texture, int id);

// Get Vehicle Texture
Image* get_Vehicle_Texture(int socket, int id);
