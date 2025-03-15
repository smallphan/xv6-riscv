#include "kernel/types.h"
#include "user/user.h"

int 
main(int argc, char * argv[]) {
  printf("Hello, world!\n");
  printf("PID of this program: %d\n", getpid());
  exit(0);
}