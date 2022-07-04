#include "generator.h"
#include "thread.h"
#include "print.h"

#include <time.h>
#include <stdio.h>
#include <sys/time.h>
#include <math.h>
#include <string.h>
#include <errno.h>



volatile genParams_t newParams[NCHAN];           // new parameters set by main thread
volatile uint32_t newParamFlags;                 // bit set by main thread for each new params
atomic_flag newParamsLock = ATOMIC_FLAG_INIT;    // lock protecting newParams access

genParams_t genParams[NCHAN];                    // currently active genParams 
double alpha = 0.1;                              // coefficient for exponentialy decaying weight (0 < alpha < 1)
volatile int gpioReg;
volatile double frequencyMean;                   // mean frequency with exponentialy decaying weighting
volatile double frequencyVariance;               // frequency variance with exponentialy decaying weighting
char stats[16];
volatile uint64_t dummy; 

#define PAUSE_VALUE 9500

uint64_t getTimeStamp() {
  struct timespec t;
  if(clock_gettime(CLOCK_MONOTONIC_COARSE, &t) < 0) {
    printErr("getTimeStamp: %s\n", strerror(errno));
    return 0;
  }
  return (uint64_t)t.tv_sec * 1000000000 + (uint64_t)t.tv_nsec;
}

void* generator(void *unused) {
  // printThreadSched("generator info: started");

  // reset global state
  bzero(genParams, sizeof(genParams));
  for(int ch = 0; ch < NCHAN; ch++) // because bzero doesn't work on volatile
    newParams[ch] = genParams[ch];
  frequencyMean = 0;
  frequencyVariance = 0;

  // loop forever
  uint64_t begin_time = getTimeStamp();
  while(1) {
    // get new params if any
    while(atomic_flag_test_and_set(&newParamsLock));
    uint16_t flags = newParamFlags;
    if (flags != 0) {
      if ((flags & ((1<<NCHAN)-1)) != 0)
        for(int i = 0; i < NCHAN; i++) {
          // print("y0[%d] = %.3f\n", i, newParams[i].y0);
          if (flags & 1)
            genParams[i] = newParams[i];
          flags >>= 1;
        }
      else 
        flags >>= NCHAN;
      if (flags & 1) {
        // request to stop the generator
        atomic_flag_clear(&newParamsLock);
        break;
      }
      newParamFlags = 0;
    }
    atomic_flag_clear(&newParamsLock);

    // for each pwm values
    int pwmval[NCHAN];
    // compute the channel values
    for(int ch = 0; ch < NCHAN; ch++) {
      double y = genParams[ch].y, dy = genParams[ch].dy;
      double val = genParams[ch].y0 + y;
      if (dy == 0) {
        // constant or sinusoidal values
        double x = genParams[ch].x, c = genParams[ch].c, s = genParams[ch].s;
        genParams[ch].y = y*c + x*s; // c and s are premultiplied by a
        genParams[ch].x = x*c - y*s;
      } else {
        // triangular values
        y += dy;
        double a = genParams[ch].a; 
        if (y > a) {
          y = 2*a - y;
          genParams[ch].dy = -dy;
        } else if (y < -a) {
          y = -2*a - y; 
          genParams[ch].dy = -dy;
        }
        genParams[ch].y = y;
      }
      //print("ch=%d val=%.3f int=%d\n", ch, val, -1-(int)(val*MAX_VALUE+.5));
      pwmval[ch] = -1-(int)(val*MAX_VALUE+.5);
    }
    // generate the pwm value
    for(int i = 0; i < MAX_VALUE; i++) {
      uint32_t flag = 0;
      for(int ch = 0; ch < NCHAN; ch++) {
        pwmval[ch]++;
        flag |= gpioBits[ch] & (pwmval[ch]>>(sizeof(int)*8-1));
      }
      //print("chunkIter=%d flag=%08X\n", chunkIter, flag);
      for(int k = 0; k < PAUSE_VALUE; k++)
        dummy++;
      *gpioSet = flag;
      *gpioClr = flag^CHAN_MASK;
    }
  
    // update the mean frequency and its variance
    // see: https://forge.in2p3.fr/dmsf/files/17104/view
    uint64_t end_time = getTimeStamp();
    double timeDiff = (double)(end_time - begin_time) *1e-9, frequency;
    // print("debug: timeDiff: %fs\n", timeDiff);
    begin_time = end_time;
    if(timeDiff != 0)
      frequency = 1/timeDiff;
    else
      frequency = 0;
    if(frequencyMean == 0) {
      frequencyMean = frequency;
      frequencyVariance = 0;
    } else {
      double mean = frequencyMean;
      double delta = frequency - mean;
      double incr = alpha * delta;
      mean += incr;
      frequencyVariance = (1 - alpha)*(frequencyVariance + delta*incr);
      frequencyMean = mean;
    }
    // print("debug: frequency: mean: %.2f Hz stdDev: %.2f \n", frequencyMean, sqrt(frequencyVariance));
  }
  // print("generator info: stopped\n");
  return NULL;
}


