#include <stdio.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "common.h"

int log_sem_init() {
  int log_sem = semget(SEM_LOG, 1, 0666 | IPC_CREAT | IPC_EXCL);
  if(-1 != log_sem) {
    short semval[1] = { 1 };
    semctl(log_sem, 0, SETVAL, semval);
  }
  else {
    log_sem = semget(SEM_LOG, 1, 0666);
  }
  return log_sem;
}

int repo_sem_init() {
  int repo_sem = semget(SEM_REPO, 1, 0666 | IPC_CREAT | IPC_EXCL);
  if(-1 != repo_sem) {
    short semval[1] = { 0 };
    semctl(repo_sem, 0, SETVAL, semval);
  }
  else {
    repo_sem = semget(SEM_REPO, 1, 0666);
  }
  return repo_sem;
}

void repo_init(int *repo_id, int *repo_sem, int *log_sem) {
  *repo_sem = repo_sem_init();
  *log_sem  = log_sem_init();
}

int main(int argc, char *argv[]) {
  int repo_id, repo_sem, log_sem;
  repo_init(&repo_id, &repo_sem, &log_sem);
  printf("repo sem: %d\nlog sem: %d\n", repo_sem, log_sem);
  return 0;
}
