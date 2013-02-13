#include <stdio.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "common.h"

int repo_sem_init(int *repo_sem) {
  int _repo_sem = semget(SEM_REPO, 1, 0666 | IPC_CREAT | IPC_EXCL);
  if(-1 != _repo_sem) {
    short semval[1] = { 0 };
    semctl(_repo_sem, 0, SETVAL, semval);
  }
  else {
    _repo_sem = semget(SEM_REPO, 1, 0666);
  }
  return _repo_sem;
}

void repo_init(int *repo_id, int *repo_sem, int *log_sem) {
  *repo_sem = repo_sem_init(repo_sem);
}

int main(int argc, char *argv[]) {
  int repo_id, repo_sem, log_sem;
  repo_init(&repo_id, &repo_sem, &log_sem);
  printf("%d\n", repo_sem);
  return 0;
}
