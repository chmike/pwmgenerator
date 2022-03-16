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


  FILE *f = fopen("/proc/sys/kernel/sched_rt_runtime_us", "w");
  if(f == NULL) {
    fprintf(stderr,"failed writing -1 to /proc/sys/kernel/sched_rt_runtime_us");
    return 1;
  }
  fprintf(f, "-1");
  fclose(f);

  f = fopen("/sys/devices/system/cpu/cpu3/cpufreq/scaling_governor", "w");
  if(f == NULL) {
    fprintf(stderr,"failed writing performance to /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor");
    return 1;
  }
  fprintf(f, "performance");
  fclose(f);

  // initialize GPIO
  int res = gpio_init();
  if(res < 0)
    exit(-1);
  if(res == 1)
    print("non-rasberry host: writing to gpio has no effect\n");


  printErr("main error: %d\n", serve(port));
  return -1;
}








