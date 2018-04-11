typedef struct client_connected{
	int id;
	Image* texture;
	sockaddr_in* addr; //to send data over udp socket to the client
	int socket_TCP  //to send data over tcp to the client(la socket ricevuta dalla accept)
}client_connected;

typedef struct thread_server_tcp_args{
    int socket_desc_tcp_client;
    client_connected* connected;
    client_disconnected* disconnected;
}thread_server_tcp_args;

typedef struct thread_server_udp_args{
    int socket_desc_udp_server;
    client_connected* connected;
    client_disconnected* disconnected;
}thread_server_udp_args;
