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

#include "common.h"

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

void repo_server_register(REPO *repo, int repo_sem) {
  repo_access_start(repo_sem);
  msgget(getpid(), 0666 | IPC_CREAT);
  msgget(SERVER_LIST_MSG_KEY, 0666 | IPC_CREAT);
  SERVER server;
  server.client_msgid = getpid();
  server.server_msgid = 0;
  server.clients = 0;
  repo->servers[repo->active_servers] = server;
  repo->active_servers++;
  repo_access_stop(repo_sem);
}

REPO *repo_get(int repo_id, int repo_sem) {
  repo_access_start(repo_sem);
  REPO *mem = shmat(repo_id, NULL, 0);
  repo_access_stop(repo_sem);
  return mem;
}

void repo_release(REPO *repo, int repo_sem, int log_sem) {
  repo_access_start(repo_sem);
  shmdt(repo);
  char msg[100];
  sprintf(msg, "DOWN: %d\n", getpid());
  log_msg(msg, log_sem);
  repo_access_stop(repo_sem);
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
    REPO *mem = shmat(repo_id, NULL, 0);
    REPO repo;
    repo.active_clients = 0;
    repo.active_rooms   = 0;
    repo.active_servers = 0;
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
    printf("Received server list request from client %d\n", req.client_msgid);
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

  sem_raise(repo_sem);
  repo = repo_get(repo_id, repo_sem);
  repo_server_register(repo, repo_sem);

  signal(SIGINT, server_exit);
  signal(SIGTERM, server_exit);

  sprintf(msg, "ALIVE: %d\n", getpid());
  log_msg(msg, log_sem);

  while(1) {
    receive_server_list_requests(repo, repo_sem);
  }

  repo_release(repo, repo_sem, log_sem);

  return 0;
}
