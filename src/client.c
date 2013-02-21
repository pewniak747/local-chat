#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "common.h"

int child_pid = -1, client_pid = -1;

int server_list(SERVER_LIST_RESPONSE *servers) {
  int msgq_id, my_msgq_id;
  msgq_id = msgget(SERVER_LIST_MSG_KEY, 0666);
  if(-1 != msgq_id) {
    SERVER_LIST_REQUEST req;
    req.type = SERVER_LIST;
    req.client_msgid = getpid();
    msgsnd(msgq_id, &req, sizeof(req), 0);
    my_msgq_id = msgget(getpid(), 0666 | IPC_CREAT);
    SERVER_LIST_RESPONSE res;
    msgrcv(my_msgq_id, &res, sizeof(res), SERVER_LIST, 0);
    printf("Active servers: %d\n", res.active_servers);
    int i;
    for(i = 0; i < res.active_servers; i++) {
      printf("Server %d: %d users online\n", res.servers[i], res.clients_on_servers[i]);
    }
    return 0;
  }
  else {
    return -1;
  }
}

void client_release() {
  kill(child_pid, SIGKILL);
  int msgq_id = msgget(getpid(), 0666);
  msgctl(msgq_id, IPC_RMID, 0);
}

void client_help() {
  printf("LOCALCHAT MANUAL:\n");
  printf("/help to show these instructions\n");
  printf("/servers to list available servers\n");
  printf("/connect <server_id> <login> to login on server\n");
  printf("/channels to list active channels\n");
  printf("/join <channel> to join channel\n");
  printf("/users to list all online users\n");
  printf("/whosthere to list users in your channel\n");
  printf("/disconnect to disconnect from a server\n");
  printf("/exit to quit\n");
}

void client_list_servers() {
  SERVER_LIST_RESPONSE servers;
  server_list(&servers);
}

int server_queue(int server_key) {
  return msgget(server_key, 0666);
}

int client_queue() {
  return msgget(client_pid, 0666 | IPC_CREAT);
}

void client_list_rooms(int *current_server, char *current_client) {
  int server_msgq = server_queue(*current_server);
  if(-1 != server_msgq) {
    CLIENT_REQUEST request;
    request.type = ROOM_LIST;
    request.client_msgid = getpid();
    strcpy(request.client_name, current_client);
    msgsnd(server_msgq, &request, sizeof(request), 0);
    ROOM_LIST_RESPONSE response;
    msgrcv(client_queue(), &response, sizeof(response), ROOM_LIST, 0);
    printf("%d channels available\n", response.active_rooms);
    int i;
    for(i = 0; i < response.active_rooms; i++) {
      printf("Channel \"%s\": %d users chatting\n", response.rooms[i].name, response.rooms[i].clients);
    }
  }
}

void client_list_global_clients(int *current_server, char *current_client) {
  int server_msgq = server_queue(*current_server);
  if(-1 != server_msgq) {
    CLIENT_REQUEST request;
    request.type = GLOBAL_CLIENT_LIST;
    request.client_msgid = getpid();
    strcpy(request.client_name, current_client);
    msgsnd(server_msgq, &request, sizeof(request), 0);
    CLIENT_LIST_RESPONSE response;
    msgrcv(client_queue(), &response, sizeof(response), GLOBAL_CLIENT_LIST, 0);
    printf("%d users online\n", response.active_clients);
    int i;
    for(i = 0; i < response.active_clients; i++) {
      printf("* %s\n", response.names[i]);
    }
  }
}

void client_list_room_clients(int *current_server, char *current_client) {
  int server_msgq = server_queue(*current_server);
  if(-1 != server_msgq) {
    CLIENT_REQUEST request;
    request.type = ROOM_CLIENT_LIST;
    request.client_msgid = getpid();
    strcpy(request.client_name, current_client);
    msgsnd(server_msgq, &request, sizeof(request), 0);
    CLIENT_LIST_RESPONSE response;
    msgrcv(client_queue(), &response, sizeof(response), ROOM_CLIENT_LIST, 0);
    printf("%d users chatting here\n", response.active_clients);
    int i;
    for(i = 0; i < response.active_clients; i++) {
      printf("* %s\n", response.names[i]);
    }
  }
}

void client_connect(char *command, int *current_server, char *current_client) {
  int i = 0;
  char server_id[100], client_name[MAX_NAME_SIZE];
  int server_id_size = 0, client_id_size = 0;
  for(; i < strlen(command); i++) {
    if(' ' != command[i]) break;
  }
  for(; i < strlen(command); i++) {
    if(' ' == command[i]) break;
    server_id[server_id_size] = command[i];
    server_id_size++;
  }
  server_id[server_id_size] = '\0';
  for(; i < strlen(command); i++) {
    if(' ' != command[i]) break;
  }
  for(; i < strlen(command); i++) {
    client_name[client_id_size] = command[i];
    client_id_size++;
  }
  client_name[client_id_size] = '\0';
  if(0 == strlen(server_id) || 0 == strlen(client_name)) {
    printf("Incorrect syntax!\n");
    printf("/connect <server_id> <login> to login on server\n");
  }
  else if(strlen(client_name) > MAX_NAME_SIZE) {
    printf("Name too long!\n");
  }
  else {
    printf("Connecting to %s as %s...\n", server_id, client_name);
    int server_key = atoi(server_id);
    int server_msgq = server_queue(server_key);
    if(-1 != server_msgq) {
      CLIENT_REQUEST request;
      request.type = LOGIN;
      request.client_msgid = getpid();
      strcpy(request.client_name, client_name);
      msgsnd(server_msgq, &request, sizeof(request), 0);
      STATUS_RESPONSE response;
      msgrcv(client_queue(), &response, sizeof(response), STATUS, 0);
      if(RESPONSE_SERVER_FULL == response.status) {
        printf("Server full!\n");
      }
      else if(RESPONSE_CLIENT_EXISTS == response.status) {
        printf("Client with name %s already exists!\n", client_name);
      }
      else if(RESPONSE_INVALID == response.status) {
        printf("Invalid name!\n");
      }
      else if(RESPONSE_CLIENT_REGISTERED == response.status) {
        printf("Logged in successfully!\n");
        *current_server = server_key;
        strcpy(current_client, client_name);
      }
      else {
        printf("ERROR!\n");
      }
    }
    else {
      printf("Server %s does not exist!\n", server_id);
    }
  }
}

void client_change_room(char *command, int *current_server, char *current_client) {
  char room_id[MAX_NAME_SIZE];
  strcpy(room_id, command);
  printf("Joining room \"%s\"...\n", room_id);
  int server_msgq = server_queue(*current_server);
  if(-1 != server_msgq) {
    CHANGE_ROOM_REQUEST request;
    request.type = CHANGE_ROOM;
    request.client_msgid = getpid();
    strcpy(request.client_name, current_client);
    strcpy(request.room_name, room_id);
    msgsnd(server_msgq, &request, sizeof(request), 0);
    STATUS_RESPONSE response;
    msgrcv(client_queue(), &response, sizeof(response), CHANGE_ROOM, 0);
    if(RESPONSE_CHANGED_ROOM == response.status) {
      printf("Now chatting in \"%s\"\n", room_id);
    }
    else {
      printf("ERROR!\n");
    }
  }
}

void client_disconnect(int *server_id, char *current_client) {
  int server_msgq = server_queue(*server_id);
  if(-1 != server_msgq) {
    CLIENT_REQUEST request;
    request.type = LOGOUT;
    request.client_msgid = getpid();
    strcpy(request.client_name, current_client);
    msgsnd(server_msgq, &request, sizeof(request), 0);
    *server_id = 0;
    printf("Disconnected from server.\n");
  }
}

void client_exit() {
  client_release();
  printf("Exiting...\n");
  exit(0);
}

void client_message_send(char *message, int server_id, char *current_client) {
  int server_msgq = server_queue(server_id);
  if(MAX_MSG_SIZE < strlen(message)) {
    printf("Message too long!\n");
  }
  else if(-1 != server_msgq) {
    TEXT_MESSAGE request;
    request.type = PUBLIC;
    request.from_id = getpid();
    strcpy(request.from_name, current_client);
    strcpy(request.text, message);
    msgsnd(server_msgq, &request, sizeof(request), 0);
  }
}

void client_receive_public_messages() {
  TEXT_MESSAGE message;
  int result = msgrcv(client_queue(), &message, sizeof(message), PUBLIC, IPC_NOWAIT);
  if(-1 != result) {
    printf("> %s says: %s\n", message.from_name, message.text);
  }
}

int str_equal(char *a, char *b) {
  if(0 == strcmp(a, b))
    return 1;
  else
    return 0;
}

int str_startswith(char *a, char *b) {
  if(a == strstr(a, b))
    return 1;
  else
    return 0;
}

int client_connected(current_server) {
  if(!(current_server > 0)) {
    printf("You are not connected to a server!\n");
    return 0;
  }
  else return 1;
}

void client_ui() {
  int current_server;
  char current_client[MAX_NAME_SIZE];
  char *command = malloc((10+MAX_MSG_SIZE)*sizeof(char));

  while(1) {
    fgets(command, 10 + MAX_MSG_SIZE, stdin);
    command[strlen(command)-1] = '\0';
    if(str_startswith(command, "/")) {
      if(str_equal(command, "/help")) {
        client_help();
      }
      else if(str_equal(command, "/servers")) {
        client_list_servers();
      }
      else if(str_equal(command, "/exit")) {
        client_exit();
      }
      else if(str_equal(command, "/disconnect")) {
        if(client_connected(current_server))
          client_disconnect(&current_server, current_client);
      }
      else if(str_startswith(command, "/connect")) {
        if(current_server > 0)
          printf("You are already connected to a server!\n");
        else
          client_connect(command+strlen("/connect"), &current_server, current_client);
      }
      else if(str_startswith(command, "/join ")) {
        if(client_connected(current_server))
          client_change_room(command+strlen("/join "), &current_server, current_client);
      }
      else if(str_equal(command, "/channels")) {
        if(client_connected(current_server))
          client_list_rooms(&current_server, current_client);
      }
      else if(str_equal(command, "/users")) {
        if(client_connected(current_server))
          client_list_global_clients(&current_server, current_client);
      }
      else if(str_equal(command, "/whosthere")) {
        if(client_connected(current_server))
          client_list_room_clients(&current_server, current_client);
      }
      else {
        printf("Unknown command!\n");
        client_help();
      }
    }
    else {
      if(client_connected(current_server))
        client_message_send(command, current_server, current_client);
    }
  }
}

void client_receive() {
  while(1) {
    client_receive_public_messages();
  }
}

int main(int argc, char *argv[]) {
  client_pid = getpid();
  signal(SIGINT, client_release);
  signal(SIGTERM, client_release);

  printf("WELCOME TO LOCALCHAT\n");
  SERVER_LIST_RESPONSE servers;
  int server_status = server_list(&servers);
  if(-1 == server_status) {
    printf("Sorry, no server online...\n");
    printf("Exiting...\n");
    exit(1);
  }

  child_pid = fork();
  if(child_pid > 0) {
    client_ui();
  }
  else {
    client_receive();
  }

  client_release();
  return 0;
}
