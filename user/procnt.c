#include "kernel/types.h"
#include "user/user.h"

int 
main(int argc, char * argv[])
{
  printf("Current total number of Proccess: %d\n", procnt());
  exit(0);
}