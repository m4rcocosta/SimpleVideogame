// #include <GL/glut.h> // not needed here
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>	//htons()
#include <netinet/in.h>		//struct sockaddr_in
#include <sys/socket.h>
#include <signal.h>
#include <pthread.h>

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"
#include "utils.h"
#include "common.h"
#include "clientList.h"
#include "so_game_protocol.h"

int connect = 1, update = 1;
ClientList* users;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void handler(int signal){
	switch(signal){
		case SIGHUP:
			break;
		case SIGINT:
			connect = 0;
			update = 0;
			break;
		default:
            fprintf(stderr, "Caught wrong signal: %d\n", signal);
            return;
		}
}

void disconnect(int socket_udp, struct sockaddr_in client_addr){
    char buf_send[BUFFERSIZE];
    PacketHeader ph;
    ph.type = PostDisconnect;
    IdPacket* ip = (IdPacket*)malloc(sizeof(IdPacket));
    ip->id = -1;
    ip->header = ph;
    int size = Packet_serialize(buf_send,&(ip->header));
    int ret = sendto(socket_udp, buf_send, size, 0, (struct sockaddr*) &client_addr, (socklen_t) sizeof(client_addr));
    Packet_free(&(ip->header));
    if(DEBUG) printf("[UDP] Sent PostDisconnect packet of %d bytes to unrecognized user.\n",ret);
}

int udp_handler(int socket_udp,char* buf_rcv,struct sockaddr_in client_addr){
	PacketHeader* ph = (PacketHeader*) buf_recv;
	if(ph->type == VehicleUpdate){
		VehicleUpdatePacket* vup = (VehicleUpdate*)Packet_deserialize(buf_recv, ph->size);
		pthread_mutex_lock(&mutex);
		ClientListElement* user = clientList_find(user, vup->id);
		if(user == NULL){
			if(DEBUG) printf("[UDP] Cannot find the user with id: %d.\n", vup->id);
			Packet_free(&vup->header);
			disconnect(socket_udp, client_addr);
			pthread_mutex_unlock(&mutex);
			return -1;
		}
		if(!user->inside_world){
			if(DEBUG) printf("[UDP] The vehicle of %d isn't inside the game.\n", vup->id);
			pthread_mutex_unlock(&mutex);
			return 0;
		}
		user->prev_x = user->x;
		user->prev_y = user->y;
        user->user_addr = client_addr;
        if(DEBUG) printf("[UDP] VehicleUpdatePacket with force_translational_update: %f force_rotation_update: %f.\n",vup->translational_force,vup->rotational_force);
        Packet_free(&vup->header);
        return 0;
	}
	return -1;
}

//Receive and apply VehicleUpdatePacket from clients
void* thread_server_udp_rec(void* args){
	int socket_udp = args->socket_desc_udp_server;
	while(connect && update){
		char buf_recv[BUFFERSIZE];
		struct sockaddr_in client_addr{0};
		int bytes_read = recvfrom(socket_udp,buf_recv,BUFFERSIZE,0, (struct sockaddr*)&client_addr,&addrlen);
		if(bytes_read == -1 || bytes_read == 0) break;
		PacketHeader* ph = (PacketHeader*)buf_recv;
		if(ph->size != bytes_read) continue;
		int ret = udp_handler(socket_udp, buf_recv, client_addr);
		ERROR_HELPER(ret, "[UDP] Error in udp_handler\n");
	}
	pthread_exit(NULL);	
}


void* thread_server_tcp(void* args){
	//TODO
}			

int main(int argc, char **argv) {
  if (argc<4) {
    printf("usage: %s <elevation_image> <texture_image> <port_number>\n", argv[1]);
    exit(-1);
  }
  char* elevation_filename=argv[1];
  char* texture_filename=argv[2];
  long tmp = strtol(argv[3], NULL, 0);		//tcp_port
  if (tmp < 1024 || tmp > 49151) {
	fprintf(stderr, "Use a port number between 1024 and 49151.\n");
	exit(EXIT_FAILURE);
  }
  char* vehicle_texture_filename="./images/arrow-right.ppm";
  printf("loading elevation image from %s ... ", elevation_filename);

  // load the images
  Image* surface_elevation = Image_load(elevation_filename);
  if (surface_elevation) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }


  printf("loading texture image from %s ... ", texture_filename);
  Image* surface_texture = Image_load(texture_filename);
  if (surface_texture) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }

  printf("loading vehicle texture (default) from %s ... ", vehicle_texture_filename);
  Image* vehicle_texture = Image_load(vehicle_texture_filename);
  if (vehicle_texture) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }
  
  int ret, socket_tcp, socket_udp;			//network variables
  
  //UDP socket
  if(DEBUG) printf("%s... initializing UDP\n", SERVER);
  if(DEBUG) printf("%s...Socket UDP creation\n", SERVER);
  
  socket_udp = socket(AF_INET, SOCK_DGRAM, 0);
  ERROR_HELPER(socket_udp, "Error in socket_udp creation\n");
  
  struct sockaddr_in server_addr_udp {0};
  server_addr_udp.sin_family			= AF_INET;
  server_addr_udp.sin_port				= htons(UDP_PORT);
  server_addr_udp.sin_addr.s_addr	= INADDR_ANY;
  
  int reuseaddr_opt_udp = 1;		//recover a server in case of a crash
  ret = setsockopt(socket_udp, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt_udp, sizeof(reuseaddr_opt_udp));
  ERROR_HELPER(ret, "Failed setsockopt() on server socket_udp");
  
  //bind udp
  ret = bind(socket_udp, (struct sockaddr*) &server_addr_udp, sizeof(server_addr_udp));
  ERROR_HELPER(ret, "Error in udp binding\n");
  
  if(DEBUG) printf("%s... UDP socket created\n", SERVER);
  
  if(DEBUG) printf("%s... creating threads for managing communications\n");
  
  //initializing users list
  users = malloc(sizeof(ClientList));
  clientList_init(users);
  if(DEBUG) printf("%s... users list initialized\n", SERVER);
  
  //creating thread for UDP communication
  pthread_t thread_udp;
  
  thread_server_udp_args* udp_args = (thread_server_udp_args*)malloc(sizeof(thread_server_udp_args));
  udp_args->socket_desc_udp_server = socket_udp;
  udp_args->clients = users;
  
  ret = pthread_create(&thread_udp, NULL, thread_server_udp_rec, udp_args);
  PTHREAD_ERROR_HELPER(ret, "Error in creating UDP thread\n");
  
  ret = pthread_detach(thread_udp);
  PTHREAD_ERROR_HELPER(ret, "Error in detach UDP thread\n");
  
  //TCP socket
  if(DEBUG) printf("%s... initializing TCP\n", SERVER);
  if(DEBUG) printf("%s...Socket TCP creation\n", SERVER);
  
  //socket for TCP communication
  socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_tcp, "Error in socket creation\n");
  
  struct sockaddr_in server_addr_tcp{0};
  server_addr_tcp.sin_family			= AF_INET;
  server_addr_tcp.sin_port				= htons(tmp);
  server_addr_tcp.sin_addr.s_addr	= INADDR_ANY;
  
  int reuseaddr_opt = 1;		//recover a server in case of a crash
  ret = setsockopt(server_tcp, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
  ERROR_HELPER(ret, "Failed setsockopt() on server socket_tcp");
  
  //binding
  ret = bind(socket_tcp, (struct sockaddr*) &server_addr, sizeof(server_addr);
  ERROR_HELPER(ret, "Error in binding\n");
  
  if(DEBUG) printf("%s... TCP socket created\n");
  
  //signal handlers
  struct sigaction sa;
  sa.sa_handler = handler;
  sa.sa_flags = SA_RESTART;
  
  //every signal is blocked during the handler
  sigfillset(&sa.sa_mask);
  ret=sigaction(SIGHUP, &sa, NULL);
  ERROR_HELPER(ret,"Error: cannot handle SIGHUP");
  ret=sigaction(SIGINT, &sa, NULL);
  ERROR_HELPER(ret,"Error: cannot handle SIGINT");
  
  //listen to a max of 8 connections
  ret = listen(socket_tcp, 8);
  ERROR_HELPER(ret, "Error in listen\n");
  
  while(1){
	  //socket descriptor that let me communicate with each client
	  struct sockaddr_in client_addr{0};
	  int client_socket;
	  
	  //waiting connections
	  client_socket = accept(socket_tcp, (struct sockaddr*) &client_addr, sizeof(struct sockaddr_in));
	  ERROR_HELPER(client_socket, "Error in accepting connections.\n");
	  
	  //spawn the thread that will communicate with the single client
	  pthread_t thread_tcp;
	  
	  thread_server_tcp_args* tcp_args = (thread_server_tcp_args*)malloc(sizeof(thread_server_tcp_args));
	  tcp_args->socket_desc_tcp_client = client_socket;
	  tcp_args->clients = users;
	  
	  ret = pthread_create(&thread_tcp, NULL, thread_server_tcp, tcp_args);
	  PTHREAD_ERROR_HELPER(ret, "Error creating tcp thread.\n");
	  
	  ret = pthread_join(thread_tcp, NULL);
	  PTHREAD_ERROR_HELPER(ret, "Error in join tcp thread.\n");
  }
  
  if(DEBUG) printf("%s...Freeing resources.\n", SERVER);
    
    //Destroy client list and pthread sem
    pthread_mutex_lock(&mutex);
    clientList_destroy(users);
    pthread_mutex_unlock(&mutex);
	pthread_mutex_destroy(&mutex);
    //Close descriptors
    ret = close(socket_tcp);
    ERROR_HELPER(ret,"Failed close() on server_tcp socket");
    ret = close(socket_udp);
    ERROR_HELPER(ret,"Failed close() on server_udp socket");
    Image_free(surface_elevation);
	Image_free(surface_texture);


  // not needed here
  //   // construct the world
  // World_init(&world, surface_elevation, surface_texture,  0.5, 0.5, 0.5);

  // // create a vehicle
  // vehicle=(Vehicle*) malloc(sizeof(Vehicle));
  // Vehicle_init(vehicle, &world, 0, vehicle_texture);

  // // add it to the world
  // World_addVehicle(&world, vehicle);


  
  // // initialize GL
  // glutInit(&argc, argv);
  // glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  // glutCreateWindow("main");

  // // set the callbacks
  // glutDisplayFunc(display);
  // glutIdleFunc(idle);
  // glutSpecialFunc(specialInput);
  // glutKeyboardFunc(keyPressed);
  // glutReshapeFunc(reshape);
  
  // WorldViewer_init(&viewer, &world, vehicle);

  
  // // run the main GL loop
  // glutMainLoop();

  // // check out the images not needed anymore
  // Image_free(vehicle_texture);
  // Image_free(surface_texture);
  // Image_free(surface_elevation);

  // // cleanup
  // World_destroy(&world);
  return 0;             
}
