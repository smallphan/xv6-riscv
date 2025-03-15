#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char argv[]) {
  uint64 id = shmget();

  int pid = fork();
  char *p = shmjoin(id);

  if (pid > 0) { // Parent
    wait(0);
    printf("[Parent output]: %s", p);
  } else if (pid == 0) { // Child
    char message[] = "Hello, this message is from child.\n";
    int len = strlen(message);
    for (int i = 0; i <= len; i++) p[i] = message[i];
  }
  
  exit(0);
}