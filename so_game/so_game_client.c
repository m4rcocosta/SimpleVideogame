
#include <GL/glut.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>   // struct sockaddr_in
#include <arpa/inet.h>    // htons() and inet_addr()
#include <pthread.h>
#include <sys/socket.h>
#include <signal.h>

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"
#include "so_game_protocol.h"
#include "world_viewer.c"
#include "utils.h"
#include "common.h"

// Variables
int window;
WorldViewer viewer;
World world;
Vehicle* vehicle; // The vehicle

int ret, id, socket_tcp, socket_udp;
struct sockaddr_in server_addr_tcp = {0}, server_addr_udp = {0};
char connected = 1;
Image *map_elevation, *map_texture, *my_texture;

// Clean resources
void clean_resources(void) {
  if(DEBUG) printf("%sCleaning up...\n", CLIENT);
  ret = close(socket_udp);
  ERROR_HELPER(ret, "Error while closing UDP socket.\n");
  if(DEBUG) printf("%ssocket_udp closed.\n", CLIENT);
  ret = close(socket_tcp);
  ERROR_HELPER(ret, "Error while closing TCP socket.\n");
  if(DEBUG) printf("%ssocket_tcp closed.\n", CLIENT);
  World_destroy(&world);
  if(DEBUG) printf("%sworld released\n", CLIENT);
  Image_free(map_elevation);
  Image_free(map_texture);
  Image_free(my_texture);
  if(DEBUG) printf("%smap_elevation, map_texture, my_texture released\n", CLIENT);
  exit(0);
}

// Handle Signal
void handle_signal(int signal) {
  switch (signal) {
    case SIGHUP:
      break;
    case SIGQUIT:
    case SIGTERM:
    case SIGINT:
      printf("%sClosing client.\n", CLIENT);
      connected = 0;
      sleep(1);
      clean_resources();
    default:
      fprintf(stderr, "Caught wrong signal: %d\n", signal);
      return;
  }
}

// Send Vehicle Updates
int send_updates(int socket_udp, struct sockaddr_in server_addr, int serverlength) {
  char* buf_send = (char*) malloc(BUFFERSIZE * sizeof(char));
  PacketHeader ph;
  ph.type = VehicleUpdate;
  VehicleUpdatePacket* vup = (VehicleUpdatePacket*)malloc(sizeof(VehicleUpdatePacket));
  vup->header = ph;

  // Get Forces Update
  vup->translational_force = vehicle->translational_force_update;
  vup->rotational_force = vehicle->rotational_force_update;

  // Get X, Y, Theta
  vup->x = vehicle->x;
  vup->y = vehicle->y;
  vup->theta = vehicle->theta;

  vup->id = id;
  int size = Packet_serialize(buf_send, &vup->header);
  int bytes_sent = sendto(socket_udp, buf_send, size, 0, (const struct sockaddr*)&server_addr, (socklen_t)serverlength);
  if(DEBUG) printf("%sSent a VehicleUpdatePacket of %d bytes with tf:%f rf:%f \n", CLIENT, bytes_sent, vup->translational_force, vup->rotational_force);
  Packet_free(&(vup->header));
  free(buf_send);
  if (bytes_sent < 0) return -1;
  return 0;
}

// Send VehicleUpdatePacket to server
void* send_UDP(void* args) {
  client_args udp_args = *(client_args*)args;
  struct sockaddr_in server_addr = udp_args.server_addr_udp;
  int socket_udp = udp_args.socket_udp;
  int server_length = sizeof(server_addr);
  while (connected) {
    int ret = send_updates(socket_udp, server_addr, server_length);
    if (ret == -1)
      printf("%sCannot send VehicleUpdatePacket.\n", CLIENT);
  }
  pthread_exit(NULL);
}

void* getter(void* args){
  client_args udp_args = *(client_args*)args;

	// Add user
	printf("%sAdding user with id %d.\n", CLIENT, udp_args.id);
  Vehicle* toAdd = (Vehicle*) malloc(sizeof(Vehicle));
	Image* texture_vehicle = get_Vehicle_Texture(udp_args.socket_tcp , udp_args.id);	
	Vehicle_init(toAdd, &world, udp_args.id, texture_vehicle);	
	World_addVehicle(&world, toAdd);
	pthread_exit(NULL);
}

// Receive and apply WorldUpdatePacket from server
void* receive_UDP(void* args) {
  client_args udp_args = *(client_args*)args;
  struct sockaddr_in server_addr = udp_args.server_addr_udp;
  int socket_udp = udp_args.socket_udp;
  socklen_t addrlen = sizeof(server_addr);
  int socket_tcp = udp_args.socket_tcp;
  char* receive_buffer = (char*) malloc(BUFFERSIZE * sizeof(char));
  while (connected) {
    int bytes_read = recvfrom(socket_udp, receive_buffer, BUFFERSIZE, 0, (struct sockaddr*)&server_addr, &addrlen);
    if(bytes_read == -1) {
      printf("%sError while receiving from UDP.\n", CLIENT);
      usleep(500000);
      continue;
    }
    if(bytes_read == 0) {
      usleep(500000);
      continue;
    }
    if(DEBUG) printf("%sReceived %d bytes from UDP.\n", CLIENT, bytes_read);
    PacketHeader* ph = (PacketHeader*) receive_buffer;
    if(ph->size != bytes_read) ERROR_HELPER(-1, "Error: partial UDP read!\n");
    if(ph->type == WorldUpdate) {
      client_args* udp_args;
      WorldUpdatePacket* wup = (WorldUpdatePacket*) Packet_deserialize(receive_buffer, bytes_read);
      for(int i = 0; i < wup->num_vehicles; i++) {
		  	Vehicle* v = World_getVehicle(&world, wup->updates[i].id);
		  	if(v == vehicle) continue; //Current client's vehicle
		  	if(v == NULL){ // Vehicle doesn't exist, add
			  	pthread_t get;
				  udp_args = (client_args*) malloc(sizeof(client_args));
				  udp_args->id = wup->updates[i].id;
			  	udp_args->socket_tcp = socket_tcp;
			  	ret = pthread_create(&get, NULL, getter, (void*) udp_args);
			  	PTHREAD_ERROR_HELPER(ret, "Error while creating get thread.\n");
				
			  	ret = pthread_join(get, NULL);
				  PTHREAD_ERROR_HELPER(ret, "Error in join on get thread.\n");
				
			  }
			  else{
			  	// Vehicle exists, update 
			  	v->theta 	= wup->updates[i].theta;
			  	v->x 			= wup->updates[i].x;
			  	v->y 			= wup->updates[i].y;
			  }
		  }

      if(world.vehicles.size == wup->num_vehicles) {
		   	Packet_free(&wup->header);
	    }
      else {
        Vehicle* current = (Vehicle*) world.vehicles.first;
        for(int i = 0; i < world.vehicles.size; i++){
			    int in = 0, forward = 0;
	        for(int j = 0; j < wup->num_vehicles && !in; j++){
	    		  if(current->id == wup->updates[j].id){
	    			  in = 1;
	    		  }
	    		}
			
	    		if(!in){
	    		  printf("%sDeleting user with id %d.\n", CLIENT, current->id);
	    		  Vehicle* toDelete = World_detachVehicle(&world, current);
		    	  current = (Vehicle*) current->list.next;
		    	  forward = 1;
		    	  free(toDelete);
		    	}
			
		    	if(!forward) current = (Vehicle*) current->list.next;	
	    	}
        Packet_free(&wup->header);
      }
    }
    else if(ph->type == PostDisconnect) {
      printf("%sServer disconnected... Exit!\n", CLIENT);     
      connected = 0;
      sleep(1);
      clean_resources();
    }
    else {
      printf("%sError: received unknown packet.\n", CLIENT);
      connected = 0;
      exit(-1);
    }
  }
  free(receive_buffer);
  pthread_exit(NULL);
}

int main(int argc, char **argv) {

  if (argc<3) {
    printf("Used: %s %s %s\nUsage: ./so_game_client <server_address> <player texture>\n", argv[0], argv[1], argv[1]==NULL?NULL:argv[2]);
    exit(-1);
  }

  printf("%sLoading texture image from %s ...\n", CLIENT, argv[2]);
  my_texture = Image_load(argv[2]);
  if (my_texture) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }
  printf("%sStarting... \n", CLIENT);

  // Signal handlers
  struct sigaction sa;
  sa.sa_handler = handle_signal;
  // Restart the system call, if at all possible
  sa.sa_flags = SA_RESTART;
  // Block every signal during the handler
  sigfillset(&sa.sa_mask);
  ret = sigaction(SIGHUP, &sa, NULL);
  ERROR_HELPER(ret, "Error: cannot handle SIGHUP.\n");
  ret = sigaction(SIGQUIT, &sa, NULL);
  ERROR_HELPER(ret, "Error: cannot handle SIGQUIT.\n");
  ret = sigaction(SIGTERM, &sa, NULL);
  ERROR_HELPER(ret, "Error: cannot handle SIGTERM.\n");
  ret = sigaction(SIGINT, &sa, NULL);
  ERROR_HELPER(ret, "Error: cannot handle SIGINT.\n");

  // TCP socket
  socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_tcp, "Error while creating socket_tcp");

  server_addr_tcp.sin_addr.s_addr = inet_addr(argv[1]);
  server_addr_tcp.sin_family = AF_INET;
  server_addr_tcp.sin_port = htons(TCP_PORT); //TCP port is defined in common.h

  ret = connect(socket_tcp, (struct sockaddr*) &server_addr_tcp, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "Error while connecting on socket_tcp");

  if(DEBUG) printf("%sTCP connection established...\n", CLIENT);

  // Communicating with server
  if(DEBUG) printf("%sStarting ID,map_elevation,map_texture requests.\n", CLIENT);
  id = get_ID(socket_tcp);
  if(DEBUG) printf("%sID number %d received.\n", TCP, id);
  map_elevation = get_Elevation_Map(socket_tcp, id);
  if(DEBUG) printf("%sMap elevation received.\n", TCP);
  map_texture = get_Texture_Map(socket_tcp, id);
  if(DEBUG) printf("%sMap texture received.\n", TCP);
  if(DEBUG) printf("%sSending vehicle texture...\n", CLIENT);
  send_Vehicle_Texture(socket_tcp, my_texture, id);
  if(DEBUG) printf("%sClient Vehicle texture sent.\n", TCP);

  // construct the world
  World_init(&world, map_elevation, map_texture, 0.5, 0.5, 0.5);
  vehicle = (Vehicle*)malloc(sizeof(Vehicle));
  Vehicle_init(vehicle, &world, id, my_texture);
  World_addVehicle(&world, vehicle);
  if(DEBUG) printf("%sWorld ready!\n", CLIENT);

  // UDP socket
  socket_udp = socket(AF_INET, SOCK_DGRAM, 0);
  ERROR_HELPER(socket_udp, "Error while creating socket_udp.\n");
  if(DEBUG) printf("%sUDP socket created...\n", CLIENT);

  server_addr_udp.sin_addr.s_addr = inet_addr(argv[1]);
  server_addr_udp.sin_family = AF_INET;
  server_addr_udp.sin_port = htons(UDP_PORT);

 // Threads sender and receiver
  pthread_t sender_udp, receiver_udp;
  client_args udp_args;
  udp_args.socket_tcp = socket_tcp;
  udp_args.server_addr_udp = server_addr_udp;
  udp_args.socket_udp = socket_udp;
  udp_args.id = id;
  ret = pthread_create(&sender_udp, NULL, send_UDP, &udp_args);
  PTHREAD_ERROR_HELPER(ret, "[CLIENT] Error while creating thread sender_udp.\n");
  if(DEBUG) printf("%sThread sender_udp created.\n", CLIENT);
  ret = pthread_create(&receiver_udp, NULL, receive_UDP, &udp_args);
  PTHREAD_ERROR_HELPER(ret, "[CLIENT] Error while creating thread receiver_udp.\n");
  if(DEBUG) printf("%sThread receiver_udp created.\n", CLIENT);

  // Run
  WorldViewer_runGlobal(&world, vehicle, &argc, argv);

  ret = pthread_detach(sender_udp);
  PTHREAD_ERROR_HELPER(ret, "Error pthread_detach on thread UDP_sender.\n");
  ret = pthread_detach(receiver_udp);
  PTHREAD_ERROR_HELPER(ret, "Error pthread_detach on thread UDP_receiver.\n");
  return 0;
}
