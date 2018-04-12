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

typedef struct playerWorld{
    int id_list[WORLDSIZE];
    int players_online;
    Vehicle** vehicles;
}localWorld;


