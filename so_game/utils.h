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

typedef struct client_args{
    int socket_tcp;                   //Socket descriptor for TCP comunication
    int socket_udp;                   //Socket descriptor for UDP comunication
    int id;                           //ID received from server
    Vehicle v;                        //Client's vehicle
    Image* map_texture;               //Map texture
    struct sockaddr_in server_addr;
}client_args;

//TCP receive function
int receive_tcp(int socket, void *buffer, size_t length, int flags);

//TCP send function
int send_tcp(int socket, const void *buffer, size_t length, int flags);

//UDP receive function
int receive_udp(int socket, void *buffer, size_t length, int flags, struct sockaddr *src_addr, socklen_t *addrlen);

//UDP send function
int send_udp(int socket, const void *buffer, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
