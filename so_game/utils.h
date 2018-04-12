#include "clientList.h"

typedef struct tcp_args{
	int client_desc;
	Image* elevation_texture;
	Image* surface_elevation;
} tcp_args;

typedef struct thread_server_tcp_args{
    int socket_desc_tcp_client;
    ClientList* connected;
}thread_server_tcp_args;

typedef struct thread_server_udp_args{
    int socket_desc_udp_server;
    ClientList* connected;
}thread_server_udp_args;

typedef struct playerWorld{
    int id_list[WORLDSIZE];
    int players_online;
    Vehicle** vehicles;
}localWorld;

typedef struct udpArgs{
    localWorld* lw;
    struct sockaddr_in server_addr;
    int socket_udp;
    int socket_tcp;
}udpArgs;
