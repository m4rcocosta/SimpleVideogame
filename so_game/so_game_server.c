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
#include <semaphore.h>

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"
#include "utils.h"
#include "common.h"
#include "clientList.h"
#include "so_game_protocol.h"

int accepted = 1, communicate = 1;
ClientList* users;
pthread_mutex_t sem_user = PTHREAD_MUTEX_INITIALIZER;
int socket_tcp, socket_udp;		//network variables
World world;

//Signal handler
void handler(int signal){
	switch(signal){
  case SIGHUP:
    break;
  case SIGINT:
    if(communicate) communicate = 0;
    printf("%s...Closing server after a %d signal...\n", SERVER, signal);
    exit(0);
  default:
    fprintf(stderr, "Caught wrong signal: %d\n", signal);
    return;
  }
}

/*
//Function to send WorldUpdatePacket updates to every client connected
void* udp_sender(void* args){
	int socket_udp = *(int*) args;
	while(communicate){
		char buf[BUFFERSIZE];
		PacketHeader ph;
		ph.type = WorldUpdate;
		WorldUpdatePacket* wup = (WorldUpdatePacket*)malloc(sizeof(WorldUpdatePacket));
		wup->header = ph;
		pthread_mutex_lock(&sem_user);
		ClientListElement* elem = users->first;
		int n = 0;
		while(elem != NULL){
			n++;
			elem = elem->next;
		}
		wup->num_vehicles = n;
		World_update(&world);
		wup->updates = (ClientUpdate*)malloc(n * sizeof(ClientUpdate));
		elem = users->first;
		int i;
		for(i = 0; elem != NULL; i++){
			ClientUpdate* cup = &(wup->updates[i]);
			cup->id = elem->id;
			cup->x = elem->vehicle->x;
			cup->y = elem->vehicle->y;
			cup->theta = elem->vehicle->theta;
			printf("%s...World updated with vehicle %d.\n", UDP, elem->id);
			elem = elem->next;
		}
		size_t packet_len = Packet_serialize(buf, &wup->header);
		elem = users->first;
		while(elem != NULL){
			int	ret = sendto(socket_udp, buf, packet_len, 0, (struct sockaddr*) &elem->user_addr, sizeof(struct sockaddr_in));
			if(ret == -1){
				printf("%s...Error in udp_sender.\n", UDP);
			}

			printf("%s...Sent WorldUpdate to %d client.\n", UDP, elem->id);
			elem = elem->next;
		}
		Packet_free(&wup->header);
		pthread_mutex_unlock(&sem_user);
	}
	pthread_exit(NULL);
}*/

//Manage VehicleUpdate packets received via UDP from the client
int udp_packet_handler(int socket_udp, char* buf, struct sockaddr_in client_addr){
	PacketHeader* ph = (PacketHeader*) buf;
	if(ph->type == VehicleUpdate){
		VehicleUpdatePacket* vup = (VehicleUpdatePacket*) Packet_deserialize(buf, ph->size);
		pthread_mutex_lock(&sem_user);
		ClientListElement* elem = clientList_find(users, vup->id);
		if(elem == NULL){
			printf("%s...Cannot find client with id %d in client list.\n", UDP, vup->id);
			Packet_free(&vup->header);
			pthread_mutex_unlock(&sem_user);
			return -1;
		}
		//update rotational and translational forces
		elem->vehicle->rotational_force_update = vup->rotational_force;
		elem->vehicle->translational_force_update = vup->translational_force;
		elem->vehicle->x = vup->x;
		elem->vehicle->y = vup->y;
		elem->vehicle->theta = vup->theta;
		World_update(&world);
		//update x_shift and y_shift
		if(elem->x >= elem->prev_x)
			elem->x_shift = elem->x - elem->prev_x;
		else
			elem->x_shift = elem->prev_x - elem->x;
		if(elem->y >= elem->prev_y)
			elem->y_shift = elem->y - elem->prev_y;
		else
			elem->y_shift = elem->prev_y - elem->y;
		elem->prev_x = elem->x;
		elem->prev_y = elem->y;

		pthread_mutex_unlock(&sem_user);
		printf("%s...Updated vehicle with id %d with rotational force: %f , translational force: %f.\n", UDP, vup->id, vup->rotational_force, vup->translational_force);
		Packet_free(&vup->header);
		return 0;
	}
	else return -1;
}

/*
//Function to receive via UDP
void* udp_receiver(void* args) {
	int socket_udp = *(int*)args;
	while(communicate){
		char buf[BUFFERSIZE];
		struct sockaddr_in client_addr;
		socklen_t client_addr_len = sizeof(client_addr);

		int bytes_read = 0;
		bytes_read = recvfrom(socket_udp, buf, BUFFERSIZE, 0, (struct sockaddr*)&client_addr, &client_addr_len);
		if(bytes_read == -1 || bytes_read == 0){
			printf("%s...Error in udp receive, exit.\n", UDP);
			pthread_exit(NULL);
		}

		PacketHeader* ph = (PacketHeader*) buf;
		if(ph->size != bytes_read) continue;
		int ret = udp_packet_handler(socket_udp, buf, client_addr);
		if(ret == -1) printf("%s...Update of type VehicleUpdate didn't work.\n", UDP);
	}
	pthread_exit(NULL);
}*/

//Manage various types of packets received from the TCP connection
int tcp_packet_handler(int tcp_socket_desc, char* buf_rec, char* buf_send, Image* surface_elevation, Image* elevation_texture) {
	PacketHeader* header = (PacketHeader*) buf_rec;
	int packet_len;
	int id = tcp_socket_desc;

	if(header->type == GetId){
		printf("%s... Received GetId request from %d user.\n", TCP, id);
		IdPacket* idp = (IdPacket*)malloc(sizeof(IdPacket));
		idp->id = id;
		PacketHeader ph;
		ph.type = GetId;
		idp->header = ph;

		packet_len = Packet_serialize(buf_send, &(idp->header));
		return packet_len;
	}
	else if(header->type == GetTexture){
		printf("%s... Received GetTexture request from %d user.\n", TCP, id);
		ImagePacket* imp = (ImagePacket*)Packet_deserialize(buf_rec, header->size);
		ImagePacket image_packet;
		PacketHeader ph;
		id = imp->id;
		
		printf("%s...Searching %d user texture.\n", TCP, id);
		pthread_mutex_lock(&sem_user);
		
		ClientListElement* elem = clientList_find(users, id);
		if(elem == NULL) return -1;

		ph.type = PostTexture;
		image_packet.header = ph;
		image_packet.id = elem->id;
		image_packet.image = elevation_texture;
		packet_len = Packet_serialize(buf_send, &(image_packet.header));
			
		pthread_mutex_unlock(&sem_user);
		return packet_len;
	}
	else if(header->type == GetElevation){
		printf("%s... Received GetElevation request from %d user.\n", TCP, id);
		ImagePacket* imp = (ImagePacket*)malloc(sizeof(ImagePacket));
		int id = imp->id;
		PacketHeader ph;
		ph.type = PostElevation;

		imp->header = ph;
		imp->id = id;
		imp->image = surface_elevation;

		packet_len = Packet_serialize(buf_send, &(imp->header));
		return packet_len;
	}
	else if(header->type == PostTexture){
		printf("%s... Received PostTexture request from %d user.\n", TCP, id);
		ImagePacket* imp = (ImagePacket*)Packet_deserialize(buf_rec, header->size);

		Vehicle* vehicle = (Vehicle*)malloc(sizeof(Vehicle));
		Vehicle_init(vehicle, &world, id, imp->image);
		World_addVehicle(&world, vehicle);

		return 0;
	}
	//Error case
	else{
		printf("%s...Unknown packet type id %d.\n", TCP, id);
		return -1;
	}
}

//Add a new user to the list when it's connected
//Function to receive/send packets from/to the client
//Remove the user when it's disconnected
void* client_thread_handler(void* args){
	tcp_args* arg = (tcp_args*)args;
	int client_desc = arg->client_desc;

	pthread_mutex_lock(&sem_user);
	printf("%s...Adding user with id %d.\n", TCP, client_desc);
	ClientListElement* user = (ClientListElement*)malloc(sizeof(ClientListElement));
	user->texture = NULL;
	user->id = client_desc;
	user->vehicle = NULL;
	user->x_shift = 0;
	user->y_shift = 0;
	user->prev_x = -1;
	user->prev_y = -1;
	clientList_add(users, user);
	printf("%s...User added successfully.\n", TCP);
	clientList_print(users);
	pthread_mutex_unlock(&sem_user);

	int ret, bytes_read, msg_len;
	char buf_recv[BUFFERSIZE], buf_send[BUFFERSIZE];
	size_t ph_len = sizeof(PacketHeader);

	while(communicate){
		//Receiving packet
		bytes_read = 0;
		bytes_read = recv(client_desc, buf_recv, ph_len, 0);
		printf("%s... %d bytes header received.\n", TCP, bytes_read);
		
		if(bytes_read == -1){
			printf("%s... Error in receiving header in tcp. Disconnection.\n", TCP);
		}

		PacketHeader* header = (PacketHeader*) buf_recv;
		while(bytes_read < header->size){
			ret = recv(client_desc, buf_recv + bytes_read, header->size - bytes_read, 0);
			if(ret == 0 || ret == -1) ERROR_HELPER(ret, "Error in receiving data in tcp.\n");
			bytes_read += ret;
		}
		printf("%s... Received %d bytes by %d.\n", TCP, bytes_read, user->id);
		
		//handler to generate the answer
		msg_len = tcp_packet_handler(client_desc, buf_recv, buf_send, arg->surface_elevation, arg->elevation_texture);
		printf("%s... Packet handler on %d user.\n", TCP, user->id);
		if(msg_len == 0) continue;
		else if(msg_len == -1) break;
		
		//send answer to the client
		printf("%s...Sending data to the client..\n", TCP);
		ret = send(client_desc, buf_send, msg_len, 0);
		if(msg_len == -1 && errno != EINTR){
			printf("%s...Error in sending data via tcp.\n", TCP);
			break;
		}
		printf("%s... Operation success! Continue...\n\n", TCP);
	}

	//if we exit from the while cicle, we have to deallocate and close
	printf("%s...User %d disconnected.\n", TCP, client_desc);
	printf("%s...Closing.\n", TCP);

	pthread_mutex_lock(&sem_user);
	ClientListElement* elem = clientList_find(users, client_desc);
	if(elem == NULL){
		pthread_mutex_unlock(&sem_user);
		close(client_desc);
		pthread_exit(NULL);
	}
	ClientListElement* canc = clientList_remove(users, elem);
	if(canc == NULL){
		pthread_mutex_unlock(&sem_user);
		close(client_desc);
		pthread_exit(NULL);
	}	
	World_detachVehicle(&world, canc->vehicle);
	free(canc->vehicle);
	Image_free(canc->texture);
	free(canc);
	printf("%s... User %d removed from list.\n", TCP, elem->id);
	clientList_print(users);
	
	pthread_mutex_unlock(&sem_user);
	close(client_desc);
	pthread_exit(NULL);
}

void* udp_handler(void* args){
	struct sockaddr_in client_addr;
	int id = *(int*) args;
	char buf_rec[BUFFERSIZE], buf_send[BUFFERSIZE];
	int ret = 0, msg_len = 0;
	socklen_t addrlen;
	
	//socket udp
	socket_udp = socket(AF_INET, SOCK_DGRAM, 0);
	ERROR_HELPER(ret, "Error in udp socket creation.\n");
	
	//Reusable socket in case of crash
	int reuseaddr_opt = 1;
	
	ret = setsockopt(socket_udp, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
	ERROR_HELPER(ret, "Error in setsockopt.\n");
	
	//Binding
	struct sockaddr_in my_addr = {0};
	my_addr.sin_family			= AF_INET;
	my_addr.sin_port			= htons(UDP_PORT);
	my_addr.sin_addr.s_addr	= INADDR_ANY;
		
	ret = bind(socket_udp, (struct sockaddr*) &my_addr, sizeof(my_addr));
	ERROR_HELPER(ret, "Error in binding.\n");
	
	while(communicate){	
		addrlen = sizeof(struct sockaddr);
		ret = recvfrom(socket_udp, buf_rec, BUFFERSIZE, 0, (struct sockaddr*)&client_addr, &addrlen);
		if(ret == -1 || ret == 0){
			printf("%s...Error in udp receive, exit.\n", UDP);
			pthread_exit(NULL);
		}
		
		udp_packet_handler(socket_udp, buf_rec, client_addr);
		if(!communicate) break;
			
		//Create the packet to send
		WorldUpdatePacket* wup = (WorldUpdatePacket*)malloc(sizeof(WorldUpdatePacket));
		PacketHeader ph;
			
		ph.type = WorldUpdate;
		wup->header = ph;
			
		wup->num_vehicles = world.vehicles.size;
		wup->updates = (ClientUpdate*)malloc(wup->num_vehicles * sizeof(ClientUpdate));
		Vehicle* curr = (Vehicle*) world.vehicles.first;
		if(curr == NULL) exit(EXIT_FAILURE);
		for(int i = 0; i < wup->num_vehicles; i++){
			wup->updates[i].id 		= curr->id;
			wup->updates[i].theta = curr->theta;
			wup->updates[i].x 		= curr->x;
			wup->updates[i].y 		= curr->y;
			ListItem* l = &curr->list;
			curr = (Vehicle*) l->next;
		}

		
		//send the packet
		msg_len = Packet_serialize(buf_send, &wup->header);
		if(communicate){
			curr = (Vehicle*) World_getVehicle(&world, id);	
			addrlen = sizeof(struct sockaddr);
			ret = sendto(socket_udp, buf_send, msg_len, 0, (struct sockaddr*)&client_addr, addrlen);
			if(ret == -1) pthread_exit(NULL);
		}
		
		//Free resources
		Packet_free(&wup->header);
	}
	
	close(socket_udp);
	printf("%s...UDP communication with id %d terminated. Success!\n", UDP, id);
	pthread_exit(NULL);
}

//Function to accept clients TCP connection
//Spawn a new thread for every client connected
void* thread_server_tcp(void* args){
	int ret;
	tcp_args* tcp_arg = (tcp_args*) args;

	while(accepted){
		struct sockaddr_in client_addr;
		socklen_t cli_addr_size = sizeof(client_addr);
		int socket_desc_tcp = accept(socket_tcp,
                                 (struct sockaddr*) &client_addr,
                                 &cli_addr_size);
		ERROR_HELPER(socket_desc_tcp, "Error in accept tcp connection.\n");

		//client thread args
		tcp_args client_args;
		client_args.client_desc = socket_desc_tcp;
		client_args.elevation_texture = tcp_arg->elevation_texture;
		client_args.surface_elevation = tcp_arg->surface_elevation;

		pthread_t client_thread, udp_handler_thread;
		//thread creation
		ret = pthread_create(&client_thread, NULL, client_thread_handler, &client_args);
		PTHREAD_ERROR_HELPER(ret, "Error in spawning client thread tcp.\n");

		//we don't wait for client thread, detach
		ret = pthread_join(client_thread, NULL);
		PTHREAD_ERROR_HELPER(ret, "Error in detach client thread tcp.\n");
		
		ret = pthread_create(&udp_handler_thread, NULL, udp_handler, &socket_desc_tcp);
		PTHREAD_ERROR_HELPER(ret, "Error in creating UDP receiver thread\n");

		ret = pthread_detach(udp_handler_thread);	//we don't wait for this thread, detach
		PTHREAD_ERROR_HELPER(ret, "Error in detach UDP receiver thread\n");
	}

	pthread_exit(NULL);
}


int main(int argc, char **argv) {

	char *elevation_filename = NULL, *texture_filename = NULL;

	if (argc == 1) {
    elevation_filename = MAP_ELEVATION;
    texture_filename = MAP_TEXTURE;
  }

  else if (argc == 2) {
    elevation_filename = argv[1];
    texture_filename = MAP_TEXTURE;
  }

  else if (argc == 3) {
    elevation_filename = argv[1];
    texture_filename = argv[2];
  }

	else {
    printf("Error: insert <elevation_image> (Default:'images/maze.pgm') <texture_image> (Default: 'images/maze.ppm')\n");
  }

  char* vehicle_texture_filename = VEHICLE;
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

  int ret;

  //TCP socket
  printf("%s... initializing TCP\n", SERVER);
  printf("%s...Socket TCP creation\n", SERVER);

  //socket for TCP communication
  socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_tcp, "Error in socket creation\n");

  printf("server TCP Port: %d\n", TCP_PORT);

  struct sockaddr_in server_addr_tcp = {0};
  server_addr_tcp.sin_family			= AF_INET;
  server_addr_tcp.sin_port				= htons(TCP_PORT);
  server_addr_tcp.sin_addr.s_addr	= INADDR_ANY;

  int reuseaddr_opt = 1;		//recover a server in case of a crash
  ret = setsockopt(socket_tcp, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
  ERROR_HELPER(ret, "Failed setsockopt() on server socket_tcp");

  //binding
  ret = bind(socket_tcp, (struct sockaddr*) &server_addr_tcp, sizeof(server_addr_tcp));
  ERROR_HELPER(ret, "Error in binding\n");

  printf("%s... TCP socket created\n", SERVER);

/*  //UDP socket
  printf("%s... initializing UDP\n", SERVER);
  printf("%s...Socket UDP creation\n", SERVER);

  socket_udp = socket(AF_INET, SOCK_DGRAM, 0);
  ERROR_HELPER(socket_udp, "Error in socket_udp creation\n");

  struct sockaddr_in server_addr_udp = {0};
  server_addr_udp.sin_family			= AF_INET;
  server_addr_udp.sin_port				= htons(UDP_PORT);
  server_addr_udp.sin_addr.s_addr	= INADDR_ANY;

  int reuseaddr_opt_udp = 1;		//recover a server in case of a crash
  ret = setsockopt(socket_udp, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt_udp, sizeof(reuseaddr_opt_udp));
  ERROR_HELPER(ret, "Failed setsockopt() on server socket_udp");

  //bind udp
  ret = bind(socket_udp, (struct sockaddr*) &server_addr_udp, sizeof(server_addr_udp));
  ERROR_HELPER(ret, "Error in udp binding\n");

  printf("%s... UDP socket created\n", SERVER); */

  //initializing users list
  users = malloc(sizeof(ClientList));
  clientList_init(users);
  printf("%s... users list initialized\n", SERVER);

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

  printf("%s...Server started.\n", SERVER);

  World_init(&world, surface_elevation, surface_texture,  0.5, 0.5, 0.5);

  tcp_args tcp_arg;
  tcp_arg.elevation_texture = surface_texture;
  tcp_arg.surface_elevation = surface_elevation;

  //create threads for udp and tcp communication
  pthread_t tcp_connect;

  ret = pthread_create(&tcp_connect, NULL, thread_server_tcp, &tcp_arg);
  PTHREAD_ERROR_HELPER(ret, "Error in spawning tcp thread.\n");

  ret = pthread_join(tcp_connect, NULL);
  PTHREAD_ERROR_HELPER(ret, "Error in join tcp thread.\n");

  /*ret = pthread_create(&udp_handler_thread, NULL, udp_handler, &socket_udp);
  PTHREAD_ERROR_HELPER(ret, "Error in creating UDP receiver thread\n");

  ret = pthread_detach(udp_handler_thread);	//we don't wait for this thread, detach
  PTHREAD_ERROR_HELPER(ret, "Error in detach UDP receiver thread\n");*/

  /*ret = pthread_create(&thread_udp_receiver, NULL, udp_receiver, &socket_udp);
  PTHREAD_ERROR_HELPER(ret, "Error in creating UDP receiver thread\n");

  ret = pthread_create(&thread_udp_sender, NULL, udp_sender, &socket_udp);
  PTHREAD_ERROR_HELPER(ret, "Error in creating UDP sender thread\n");*/


  /*ret = pthread_detach(thread_udp_receiver);	//we don't wait for this thread, detach
  PTHREAD_ERROR_HELPER(ret, "Error in detach UDP receiver thread\n");

  ret = pthread_detach(thread_udp_sender);		//we don't wait for this thread, detach
  PTHREAD_ERROR_HELPER(ret, "Error in detach UDP sender thread\n");*/



  printf("%s...Freeing resources.\n", SERVER);

  //Destroy client list and pthread sem
  pthread_mutex_destroy(&sem_user);
  clientList_destroy(users);
  //Close descriptors
  ret = close(socket_tcp);
  ERROR_HELPER(ret,"Failed closing server_tcp socket");
  ret = close(socket_udp);
  ERROR_HELPER(ret,"Failed closing server_udp socket");
  Image_free(vehicle_texture);
  Image_free(surface_elevation);
  Image_free(surface_texture);

  World_destroy(&world);

  exit(EXIT_SUCCESS);
}
