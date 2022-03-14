#include "gpio.h"
#include "generator.h"
#include "thread.h"
#include "server.h"
#include "hexdump.h"
#include "print.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>   // needed for sin and cos TBR
#include <unistd.h> // needed for sleep TBR
#include <stdint.h>

#define UNUSED(x) (void)(x)

// compile : gcc -I. *.c -O3 -latomic -lm -lpthread -Wall && sudo ./a.out 4000

int main(int argc, char *argv[]) {
  int port = 1234;
  if(argc > 1) {
    port = atoi(argv[1]);
    if(port <= 1024)
      port = 1234;
  } else
    port = 1234;

  // initialize GPIO
  int res = gpio_init();
  if(res < 0)
    exit(-1);
  if(res == 1)
    print("non-rasberry host: writing to gpio has no effect\n");


  printErr("main error: %d\n", serve(port));
  return -1;
}








