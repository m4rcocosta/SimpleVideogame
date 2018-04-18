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

int accept = 1, update = 0, communicate = 1;
ClientList* users;
sem_t* sem_user;
int socket_tcp, socket_udp;		//network variables
World world;

//Signal handler
void handler(int signal){
	switch(signal){
		case SIGHUP:
			break;
		case SIGINT:
			if(communicate) communicate = 0;
			if(update) update = 0;
			printf("%s...Closing server after a %s signal...\n", SERVER, signal);
			break;
		default:
            fprintf(stderr, "Caught wrong signal: %d\n", signal);
            return;
		}
}

//Function to send WorldUpdatePacket updates to every client connected
void* udp_sender(void* args){
	int socket_udp = *(int*) args;
	while(communicate){
		char buf[BUFFERSIZE];
		PacketHeader ph;
		ph->type = WorldUpdate;
		WorldUpdatePacket* wup = (WorldUpdatePacket*)malloc(sizeof(WorldUpdatePacket));
		wup->header = ph;
		sem_wait(&sem_user);
		ClientListElement* elem = users->first;
		int n = 0;
		while(elem != NULL){
			n++;
			elem = elem->next;
		}
		if(n == 0){
			printf("%s...No client found in this moment, refresh.\n", UDP);
			sem_post(sem_user);
			continue;
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
		int packet_len = Packet_serialize(buf, &wup->header);
		elem = users->first;
		while(elem != NULL){
			int ret = sendto(socket_udp, buf, packet_len, 0, (struct sockadrr*) &elem->user_addr, sizeof(struct sockaddr_in));
			ERROR_HELPER(ret, "Error in UDPSender.\n");
			printf("%s...Sent WorldUpdate to %d client.\n", UDP, elem->id);
			elem = elem->next;
		}
		Packet_free(&wup->header);
		sem_post(sem_user);
	}
	pthread_exit(NULL);
}

//Manage VehicleUpdate packets received via UDP from the client
int udp_packet_handler(int socket_udp, char* buf, struct sockaddr_in client_addr){
	//to do
} 

//Function to receive via UDP 
void* udp_receiver(void* args){
	int socket_udp = *(int*)args;
	while(communicate){
		char buf[BUFFERSIZE];
		struct sockaddr_in client_addr{0};
		int read = recvfrom(socket_udp, buf, BUFFERSIZE, 0, (struct sockaddr*) &client_addr, sizeof(struct sockaddr_in));
		if(read == -1 || read == 0) break;
		PacketHeader* ph = (PacketHeader*) buf;
		if(ph->size != read) continue;
		int ret = udp_packet_handler(socket_udp, buf, client_addr);
		if(ret == -1) printf("%s...Update of type VehicleUpdate didn't work.\n", UDP);
	}
	pthread_exit(NULL);
}

//Manage various types of packets received from the TCP connection
int tcp_packet_handler(int tcp_socket_desc, int id, char* buf, Image* surface_elevation, Image* elevation_texture){
	PacketHeader* header = (PacketHeader*) buf;
	
	if(header->type == GetId){
		IdPacket* idp = (IdPacket*)malloc(sizeof(IdPacket));
		idp->id = id;
		PacketHeader ph;
		ph->type = GetId;
		idp->header = ph;
		
		char buf_send[BUFFERSIZE];
		int packet_len = Packet_serialize(buf_send, &(idp->header));
		
		//send the packet via socket
		int bytes_sent = 0, ret;
		while(bytes_sent < packet_len){
			ret = send(tcp_socket_desc, buf_send + bytes_sent, packet_len - bytes_sent,0);
			if(ret == -1 && errno == EINTR) continue;
			ERROR_HELPER(ret, "Error in id communication.\n");
			if(ret == 0) break;
			bytes_sent += ret;
		}
		
		Packet_free(&(idp->header));
		free(idp);
		
		printf("%s...Sent %d bytes.\n", TCP, bytes_sent);
		return 1;
	}
	
	else if(header->type == GetTexture){
		ImagePacket* imp = (ImagePacket*) buf;
		int id = imp->id;
		
		//header for the answer
		PacketHeader ph;
		ph->type = PostTexture;
		
		//packet to send texture to client
		ImagePacket* text_send = (ImagePacket*)malloc(sizeof(ImagePacket));
		text_send->header = ph;
		text_send->id = id;
		text_send->image = elevation_texture;
		
		char buf_send[BUFFERSIZE];
		int packet_len = Packet_serialize(buf_send, &(imp->header));
		int bytes_sent = 0, ret;
		while(bytes_sent < packet_len){
			ret = send(tcp_socket_desc, buf_send + bytes_sent, packt_len - bytes_sent,0);
			if (ret == -1 && errno == EINTR) continue;
			ERROR_HELPER(ret, "Error in texture request.\n");
			if (ret == 0) break;
			bytes_sent += ret;
		}

		Packet_free(&(text_send->header));
		free(text_send);
		
		printf("%s...Texture sent to id %d.\n", TCP, id);
		return 1;
	}
	else if(header->type == GetElevation){
		ImagePacket* imp = (ImagePacket*) buf;
		int id = imp->id;
		PacketHeader ph;
		ph->type = PostElevation;
		
		//packet to sent elevation to client
		ImagePacket* elev = (ImagePacket*)malloc(sizeof(ImagePacket));
		elev->header = ph;
		elev->id = id;
		elev->image = surface_elevation;
		
		int bytes_sent = 0, ret;
		int packet_len = Packet_serialize(buf_send, &(elev->header));
        while(bytes_sent < packet_len){
			ret = send(tcp_socket_desc, buf_send + bytes_sent, packet_len - bytes_sent,0);
			if (ret == -1 && errno == EINTR) continue;
			ERROR_HELPER(ret, "Error in elevation request.\n");
			if (ret == 0) break;
			bytes_sent += ret;
		}

		Packet_free(&(elev->header));
		free(elev);
		
		printf("%s...Sent texture to id %d.\n",TCP, id);
		return 1;
	}
	else if(header->type == PostTexture){
		PacketHeader* ph = Packet_deserialize(buf, header->size);
		ImagePacket* imp = (ImagePacket*) ph;
		
		Vehicle* vehicle = (Vehicle*)malloc(sizeof(Vehicle));
		Vehicle_init(vehicle, &world, id, imp->image);
		World_addVehicle(&world, vehicle);
		
		Packet_free(ph);
		free(imp);
		printf("%s...Texture received id %d.\n", TCP, id);
		return 1;
	}
	//error case
	else{
		printf("%s...Unknown packet type id %d.\n", TCP, id);
		return -1;
	}
	return -1;
}

//Add a new user to the list when it's connected
//Function to receive packets from the client
//Remove the user when it's disconnected
void* client_thread_handler(void* args){
	tcp_args* args = (tcp_args*)args;
	int client_desc = args->client_desc;
	
	sem_wait(sem_user);
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
	sem_post(sem_user);
	
	int msg_len = 0, ret;
	char buf_recv[BUFFERSIZE];
	int ph_len = sizeof(PacketHeader);
	
	while(communicate){
		//Receiving packet
		while(msg_len < ph_len){
			ret = recv(client_desc, buf_recv + msg_len, ph_len - msg_len, 0);
			if(ret == -1 && errno == EINTR) continue;
			ERROR_HELPER(ret, "Error in receiving data in tcp.\n");
			msg_len += ret;
		}
		
		PacketHeader* header = (PacketHeader*) buf_recv;
		int size = header->size - ph_len;
		msg_len = 0;
		
		while(msg_len < size){
			ret = recv(client_desc, buf_recv + msg_len + ph_len, size - msg_len, 0);
			if (ret==-1 && errno == EINTR) continue;
			ERROR_HELPER(ret, "Error in receiving data in tcp.\n");
			msg_len += ret;
		}
		
		ret = tcp_packet_handler(client_desc, args->client_desc, buf_recv, args->surface_elevation, args->elevation_texture);
		if(ret == 1) printf("%s...Packet managed successfully.\n", TCP);
		else printf("%s...Failure in managing packet\n", TCP);
	}
	
	//if we exit from the while cicle, we have to deallocate and close
	printf("%s...User %d disconnected.\n", TCP, client_desc);
	printf("%s...Closing.\n", TCP);
	
	sem_wait(sem_user);
	ClientListElement* elem = clientList_find(users, client_desc);
	if(elem == NULL){
		sem_post(sem_user);
		close(client_desc);
		pthread_exit(NULL);
	}
	ClientListElement* canc = clientList_remove(users, elem);
	if(canc == NULL){
		sem_post(sem_user);
		close(client_desc);
		pthread_exit(NULL);
	}
	World_detachVehicle(canc->vehicle);
	free(canc->vehicle);
	Image_free(canc->texture);
	free(canc);
	sem_post(sem_user);
	close(client_desc);
	pthread_exit(NULL);
}

//Function to accept clients TCP connection
//Spawn a new thread for every client connected
void* thread_server_tcp(void* args){
	int ret;
	tcp_args* tcp_arg = (tcp_args*) args;
	int client_desc = args->client_desc;
	struct sockaddr_in client_addr{0};
	pthread_t client_thread;
	
	while(accept){
		int socket_desc_tcp = accept(socket_tcp, (struct sockaddr*) &client_addr, sizeof(struct sockaddr_in));
		ERROR_HELPER(socket_desc_tcp, "Error in accept tcp connection.\n");
		
		//client thread args
		tcp_args client_args;
		client_args->client_desc = socket_desc_tcp;
		client_args->elevation_texture = tcp_arg->elevation_texture;
		client_args->surface_elevation = tcp_arg->surface_elevation;
		
		//thread creation
		ret = pthread_create(client_thread, NULL, client_thread_handler, &client_args);
		PTHREAD_ERROR_HELPER(ret, "Error in spawning client thread tcp.\n");
		
		//we don't wait for client thread, detach
		ret = pthread_detach(client_thread);
		PTHREAD_ERROR_HELPER(ret, "Error in detach client thread tcp.\n");
	}
	
	pthread_exit(NULL);
}			

int main(int argc, char **argv) {
  if (argc<4) {
    printf("usage: %s <elevation_image> <texture_image>\n", argv[1]);
    exit(-1);
  }
  char* elevation_filename=argv[1];
  char* texture_filename=argv[2];
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
  
  int ret;
  
  //UDP socket
  printf("%s... initializing UDP\n", SERVER);
  printf("%s...Socket UDP creation\n", SERVER);
  
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
  
  printf("%s... UDP socket created\n", SERVER);
  
  printf("%s... creating threads for managing communications\n");
  
  //initializing users list
  users = malloc(sizeof(ClientList));
  clientList_init(users);
  printf("%s... users list initialized\n", SERVER);
  
  //creating thread for UDP communication
  pthread_t thread_udp_sender, thread_udp_receiver;
  
  /*udp_args udp_arg;
  udp_arg->surface_texture = surface_texture;
  udp_arg->surface_elevation = surface_elevation;
  udp_arg->vehicle_texture = vehicle_texture;
  */
  
  ret = pthread_create(&thread_udp_receiver, NULL, udp_receiver, &socket_udp);
  PTHREAD_ERROR_HELPER(ret, "Error in creating UDP receiver thread\n");
  
  ret = pthread_create(&thread_udp_sender, NULL, thread_server_udp, &socket_udp);
  PTHREAD_ERROR_HELPER(ret, "Error in creating UDP sender thread\n"); 
  
  ret = pthread_detach(thread_udp_receiver);	//we don't wait for this thread, detach
  PTHREAD_ERROR_HELPER(ret, "Error in detach UDP receiver thread\n");
  
  ret = pthread_detach(thread_udp_sender);		//we don't wait for this thread, detach
  PTHREAD_ERROR_HELPER(ret, "Error in detach UDP sender thread\n");
  
  //TCP socket
  printf("%s... initializing TCP\n", SERVER);
  printf("%s...Socket TCP creation\n", SERVER);
  
  //socket for TCP communication
  socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_tcp, "Error in socket creation\n");
  
  struct sockaddr_in server_addr_tcp{0};
  server_addr_tcp.sin_family			= AF_INET;
  server_addr_tcp.sin_port				= htons(TCP_PORT);
  server_addr_tcp.sin_addr.s_addr	= INADDR_ANY;
  
  int reuseaddr_opt = 1;		//recover a server in case of a crash
  ret = setsockopt(server_tcp, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
  ERROR_HELPER(ret, "Failed setsockopt() on server socket_tcp");
  
  //binding
  ret = bind(socket_tcp, (struct sockaddr*) &server_addr, sizeof(server_addr);
  ERROR_HELPER(ret, "Error in binding\n");
  
  printf("%s... TCP socket created\n");
  
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
  
  //tcp thread
  pthread_t tcp_connect;
  
  tcp_args tcp_arg;
  tcp_arg->elevation_texture = surface_texture;
  tcp_arg->surface_texture = surface_elevation;
  
  ret = pthread_create(&tcp_connect, NULL, thread_server_tcp, &tcp_arg);
  PTHREAD_ERROR_HELPER(ret, "Error in spawning tcp thread.\n"); 
  
  ret = pthread_join(tcp_connect, NULL);
  PTHREAD_ERROR_HELPER(ret, "Error in join tcp thread.\n");
  
  printf("%s...Freeing resources.\n", SERVER);
    
    //Destroy client list and pthread sem
    pthread_mutex_lock(&mutex);
    clientList_destroy(users);
    pthread_mutex_unlock(&mutex);
	pthread_mutex_destroy(&mutex);
    //Close descriptors
    ret = close(socket_tcp);
    ERROR_HELPER(ret,"Failed closing server_tcp socket");
    ret = close(socket_udp);
    ERROR_HELPER(ret,"Failed closing server_udp socket");
    Image_free(vehicle_texture);
    Image_free(surface_elevation);
	Image_free(surface_texture);
	
	World_destroy(&world);


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
