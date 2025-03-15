#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char argv[]) {

  fork();
  
  wait(0);
  int *a = malloc(sizeof(int) * 200);
  for (int i = 0; i < 200; i++) a[i] = 10;
  free(a);

  // void *a = malloc(1000);
  // void *b = malloc(500);
  // void *c = malloc(1000);
  // void *d = malloc(2000);
  // void *e = malloc(8 * 1024 * 1024 - 8);
  // free(a);
  // free(b);
  // free(c);
  // free(d);
  // free(e);
  
  exit(0);
}