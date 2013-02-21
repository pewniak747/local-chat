#include <stdio.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>

#include "common.h"

#define INFINITY INT_MAX
#define EMPTY_ROOM "zzzz"
#define EMPTY_CLIENT "zzzz"

int repo_id, repo_sem, log_sem;
REPO *repo;

void sem_lower(int sem_id) {
  struct sembuf buf;
  buf.sem_num = 0;
  buf.sem_op  = -1;
  buf.sem_flg = 0;
  semop(sem_id, &buf, 1);
}

void sem_raise(int sem_id) {
  struct sembuf buf;
  buf.sem_num = 0;
  buf.sem_op  = 1;
  buf.sem_flg = 0;
  semop(sem_id, &buf, 1);
}

void log_msg(const char message[], int log_sem) {
  sem_lower(log_sem);
  int fd = open("/tmp/czat.log", O_WRONLY|O_CREAT|O_APPEND);
  write(fd, message, strlen(message));
  close(fd);
  sem_raise(log_sem);
}

void repo_access_start(int repo_sem) {
  sem_lower(repo_sem);
}

void repo_access_stop(int repo_sem) {
  sem_raise(repo_sem);
}

int server_compare(const void *s1, const void *s2) {
  int s1id = ((SERVER*)s1)->client_msgid;
  int s2id = ((SERVER*)s2)->client_msgid;
  if(s1id > s2id) return 1;
  if(s2id > s1id) return -1;
  return 0;
}

void server_sort(REPO *repo) {
  qsort(repo->servers, MAX_SERVER_NUM, sizeof(SERVER), server_compare);
}

void server_register(REPO *repo, int repo_sem) {
  repo_access_start(repo_sem);
  msgget(getpid(), 0666 | IPC_CREAT);
  msgget(SERVER_LIST_MSG_KEY, 0666 | IPC_CREAT);
  SERVER server;
  server.client_msgid = getpid();
  server.server_msgid = 0;
  server.clients = 0;
  repo->servers[repo->active_servers] = server;
  repo->active_servers++;
  server_sort(repo);
  repo_access_stop(repo_sem);
}

SERVER* server_get(REPO *repo) {
  int i;
  for(i = 0; i < MAX_SERVER_NUM; i++) {
    if(repo->servers[i].client_msgid == getpid())  {
      return &repo->servers[i];
    }
  }
  return '\0';
}

int client_compare(const void *c1, const void *c2) {
  char *c1id = ((CLIENT*)c1)->name;
  char *c2id = ((CLIENT*)c2)->name;
  return strcmp(c1id, c2id);
}

void client_sort(REPO *repo) {
  qsort(repo->clients, MAX_CLIENTS, sizeof(CLIENT), client_compare);
}

int room_compare(const void *r1, const void *r2) {
  char *r1id = ((ROOM*)r1)->name;
  char *r2id = ((ROOM*)r2)->name;
  return strcmp(r1id, r2id);
}

void room_sort(REPO *repo) {
  qsort(repo->rooms, MAX_CLIENTS, sizeof(ROOM), room_compare);
}

ROOM* room_get(REPO *repo, char *name) {
  int i;
  for(i = 0; i < MAX_CLIENTS; i++) {
    if(0 == strcmp(repo->rooms[i].name, name))  {
      return &repo->rooms[i];
    }
  }
  return '\0';
}

CLIENT* client_get(REPO *repo, char *name) {
  int i;
  for(i = 0; i < MAX_CLIENTS; i++) {
    if(0 == strcmp(repo->clients[i].name, name))  {
      return &repo->clients[i];
    }
  }
  return '\0';
}

REPO* repo_get(int repo_id, int repo_sem) {
  repo_access_start(repo_sem);
  REPO *mem = shmat(repo_id, '\0', 0);
  repo_access_stop(repo_sem);
  return mem;
}

void repo_release(REPO *repo, int repo_sem, int log_sem) {
  repo_access_start(repo_sem);

  char msg[100];
  sprintf(msg, "DOWN: %d\n", getpid());
  log_msg(msg, log_sem);

  if(repo->active_servers == 1) {
    int msgq_id = msgget(SERVER_LIST_MSG_KEY, 0666);
    int repo_id = shmget(ID_REPO, sizeof(REPO), 0666);
    msgctl(msgq_id, IPC_RMID, 0);
    msgq_id = msgget(getpid(), 0666);
    msgctl(msgq_id, IPC_RMID, 0);
    semctl(repo_sem, IPC_RMID, 0);
    semctl(log_sem, IPC_RMID, 0);
    shmctl(repo_id, IPC_RMID, 0);
  }
  else {
    int msgq_id = msgget(getpid(), 0666);
    msgctl(msgq_id, IPC_RMID, 0);
    repo->active_servers--;
    SERVER *me = server_get(repo);
    me->client_msgid = INFINITY;
    server_sort(repo);
    shmdt(repo);
    repo_access_stop(repo_sem);
  }
}

int log_sem_init() {
  int log_sem = semget(SEM_LOG, 1, 0666 | IPC_CREAT | IPC_EXCL);
  if(-1 != log_sem) {
    semctl(log_sem, 0, SETVAL, 1);
  }
  else {
    log_sem = semget(SEM_LOG, 1, 0666);
  }
  return log_sem;
}

int repo_mem_init(int repo_sem) {
  int repo_id = shmget(ID_REPO, sizeof(REPO), 0666 | IPC_CREAT | IPC_EXCL);
  if(-1 != repo_id) {
    REPO *mem = shmat(repo_id, '\0', 0);
    REPO repo;
    repo.active_clients = 0;
    repo.active_rooms   = 0;
    repo.active_servers = 0;
    int i;
    for(i = 0; i < MAX_SERVER_NUM; i++) {
      repo.servers[i].client_msgid = INFINITY;
    }
    for(i = 0; i < MAX_CLIENTS; i++) {
      strcpy(repo.clients[i].name, EMPTY_CLIENT);
      strcpy(repo.rooms[i].name, EMPTY_ROOM);
      repo.rooms[i].clients = 0;
    }
    *mem = repo;
    shmdt(mem);
    sem_raise(repo_sem);
  }
  else {
    repo_id = shmget(ID_REPO, 1, 0666);
  }
  return repo_id;
}

int repo_sem_init() {
  int repo_sem = semget(SEM_REPO, 1, 0666 | IPC_CREAT | IPC_EXCL);
  if(-1 != repo_sem) {
    semctl(repo_sem, 0, SETVAL, 0);
  }
  else {
    repo_sem = semget(SEM_REPO, 1, 0666);
  }
  return repo_sem;
}

void repo_init(int *repo_id, int *repo_sem, int *log_sem) {
  *repo_sem = repo_sem_init();
  *repo_id  = repo_mem_init(*repo_sem);
  *log_sem  = log_sem_init();
}

void receive_server_list_requests(REPO *repo, int repo_sem) {
  SERVER_LIST_REQUEST req;
  int msgq_id = msgget(SERVER_LIST_MSG_KEY, 0666);
  int result = msgrcv(msgq_id, &req, sizeof(req), SERVER_LIST, IPC_NOWAIT);
  if(-1 != result) {
    SERVER_LIST_RESPONSE response;
    response.type = SERVER_LIST;
    response.active_servers = repo->active_servers;
    int i;
    for(i = 0; i < repo->active_servers; i++) {
      response.servers[i] = repo->servers[i].client_msgid;
      response.clients_on_servers[i] = repo->servers[i].clients;
    }
    int client_msgq_id = msgget(req.client_msgid, 0666);
    msgsnd(client_msgq_id, &response, sizeof(response), 0);
  }
}

void client_room_enter(REPO *repo, CLIENT *client, char *room_name) {
  ROOM *new_room = room_get(repo, room_name);
  if('\0' == new_room) {
    new_room = &repo->rooms[repo->active_rooms];
    strcpy(new_room->name, room_name);
    new_room->clients = 0;
    repo->active_rooms++;
  }
  new_room->clients++;
  strcpy(client->room, new_room->name);
  room_sort(repo);
}

void client_room_leave(REPO *repo, CLIENT *client, char *room_name) {
  ROOM *old_room = room_get(repo, room_name);
  old_room->clients--;
  if(0 == old_room->clients) {
     strcpy(old_room->name, EMPTY_ROOM);
     repo->active_rooms--;
  }
  room_sort(repo);
  strcpy(client->room, EMPTY_ROOM);
}

void receive_login_requests(REPO *repo, int repo_sem) {
  CLIENT_REQUEST request;
  int msgq_id = msgget(getpid(), 0666);
  int result = msgrcv(msgq_id, &request, sizeof(request), LOGIN, IPC_NOWAIT);
  if(-1 != result) {
    STATUS_RESPONSE response;
    response.type = STATUS;
    SERVER *me = server_get(repo);
    if(me->clients == SERVER_CAPACITY) {
      response.status = RESPONSE_SERVER_FULL;
    }
    else if('\0' != client_get(repo, request.client_name)) {
      response.status = RESPONSE_CLIENT_EXISTS;
    }
    else {
      CLIENT *new_client = &repo->clients[repo->active_clients];
      client_room_enter(repo, new_client, "");
      strcpy(new_client->name, request.client_name);
      new_client->server_id = getpid();
      repo->active_clients++;
      me->clients++;
      client_sort(repo);
      char msg[100];
      sprintf(msg, "LOGGED_IN@%d: %s\n", getpid(), request.client_name);
      log_msg(msg, log_sem);
      response.status = RESPONSE_CLIENT_REGISTERED;
    }
    int client_msgq_id = msgget(request.client_msgid, 0666);
    msgsnd(client_msgq_id, &response, sizeof(response), 0);
  }
}

void receive_change_room_requests(REPO *repo, int repo_sem) {
  CHANGE_ROOM_REQUEST request;
  int msgq_id = msgget(getpid(), 0666);
  int result = msgrcv(msgq_id, &request, sizeof(request), CHANGE_ROOM, IPC_NOWAIT);
  if(-1 != result) {
    STATUS_RESPONSE response;
    response.type = CHANGE_ROOM;
    CLIENT *client = client_get(repo, request.client_name);
    if('\0' != client) {
      client_room_leave(repo, client, client->room);
      client_room_enter(repo, client, request.room_name);
      response.status = RESPONSE_CHANGED_ROOM;
    }
    else {
      response.status = RESPONSE_ERROR;
    }
    int client_msgq_id = msgget(request.client_msgid, 0666);
    msgsnd(client_msgq_id, &response, sizeof(response), 0);
  }
}

void receive_list_room_requests(REPO *repo, int repo_sem) {
  CLIENT_REQUEST request;
  int msgq_id = msgget(getpid(), 0666);
  int result = msgrcv(msgq_id, &request, sizeof(request), ROOM_LIST, IPC_NOWAIT);
  if(-1 != result) {
    ROOM_LIST_RESPONSE response;
    response.type = ROOM_LIST;
    response.active_rooms = repo->active_rooms;
    memcpy(&response.rooms, repo->rooms, MAX_CLIENTS*sizeof(ROOM));
    int client_msgq_id = msgget(request.client_msgid, 0666);
    msgsnd(client_msgq_id, &response, sizeof(response), 0);
  }
}

void receive_list_global_clients_requests(REPO *repo, int repo_sem) {
  CLIENT_REQUEST request;
  int msgq_id = msgget(getpid(), 0666);
  int result = msgrcv(msgq_id, &request, sizeof(request), GLOBAL_CLIENT_LIST, IPC_NOWAIT);
  if(-1 != result) {
    CLIENT_LIST_RESPONSE response;
    response.type = GLOBAL_CLIENT_LIST;
    response.active_clients = repo->active_clients;
    int i;
    for(i = 0; i < repo->active_clients; i++) {
      strcpy(response.names[i], repo->clients[i].name);
    }
    int client_msgq_id = msgget(request.client_msgid, 0666);
    msgsnd(client_msgq_id, &response, sizeof(response), 0);
  }
}

void receive_list_room_clients_requests(REPO *repo, int repo_sem) {
  CLIENT_REQUEST request;
  int msgq_id = msgget(getpid(), 0666);
  int result = msgrcv(msgq_id, &request, sizeof(request), ROOM_CLIENT_LIST, IPC_NOWAIT);
  if(-1 != result) {
    CLIENT_LIST_RESPONSE response;
    response.type = ROOM_CLIENT_LIST;
    CLIENT *client = client_get(repo, request.client_name);
    int clients = 0, i;
    for(i = 0; i < repo->active_clients; i++) {
      if(0 == strcmp(repo->clients[i].room, client->room)) {
        strcpy(response.names[clients], repo->clients[i].name);
        clients++;
      }
    }
    response.active_clients = clients;
    int client_msgq_id = msgget(request.client_msgid, 0666);
    msgsnd(client_msgq_id, &response, sizeof(response), 0);
  }
}

void receive_logout_requests(REPO *repo, int repo_sem) {
  CLIENT_REQUEST request;
  int msgq_id = msgget(getpid(), 0666);
  int result = msgrcv(msgq_id, &request, sizeof(request), LOGOUT, IPC_NOWAIT);
  if(-1 != result) {
    SERVER *me = server_get(repo);
    CLIENT *client = client_get(repo, request.client_name);
    if('\0' != client) {
      strcpy(client->name, EMPTY_CLIENT);
      repo->active_clients--;
      me->clients--;
      client_room_leave(repo, client, client->room);
      client_sort(repo);

      char msg[100];
      sprintf(msg, "LOGGED_OUT@%d: %s\n", getpid(), request.client_name);
      log_msg(msg, log_sem);
    }
  }
}

void server_exit() {
  repo_release(repo, repo_sem, log_sem);
  exit(1);
}

int main(int argc, char *argv[]) {
  repo_init(&repo_id, &repo_sem, &log_sem);

  char msg[100];
  sprintf(msg, "UP: %d, repo sem: %d log sem: %d repo_id: %d\n", getpid(), repo_sem, log_sem, repo_id);
  log_msg(msg, log_sem);

  repo_access_stop(repo_sem);
  repo = repo_get(repo_id, repo_sem);
  server_register(repo, repo_sem);

  signal(SIGINT, server_exit);
  signal(SIGTERM, server_exit);

  sprintf(msg, "ALIVE: %d\n", getpid());
  log_msg(msg, log_sem);

  while(1) {
    repo_access_start(repo_sem);
    receive_server_list_requests(repo, repo_sem);
    receive_login_requests(repo, repo_sem);
    receive_logout_requests(repo, repo_sem);
    receive_change_room_requests(repo, repo_sem);
    receive_list_room_requests(repo, repo_sem);
    receive_list_global_clients_requests(repo, repo_sem);
    receive_list_room_clients_requests(repo, repo_sem);
    repo_access_stop(repo_sem);
  }

  repo_release(repo, repo_sem, log_sem);

  return 0;
}
