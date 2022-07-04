#ifndef GENERATOR_H
#define GENERATOR_H

#define _GNU_SOURCE
#include <stdint.h>
#include <stdatomic.h>

#include "gpio.h" // for NCHAN

#define UNUSED(x) (void)(x)

// program parameters
#define BITS_RESOLUTION 12
#define MAX_VALUE (1 << BITS_RESOLUTION)
#define CHUNK_SIZE 32000


// generator parameters for an output.
// Constant requires: x=y=dy=c=s=a=0, y0>=0, y0<=1.
// Sinusoidal requires: dy=0, x²+y²=1, c²+s²=1, a!=0, y0+a<=1, y0-a>=0.
// Triangular requires: s=c=x=0, a!=0, y>=0, y<=1, dy>0, dy<.5, y0+a<=1, y0-a>=0. 
typedef struct {
  double y0;     // offset of y0, 
  double x, y;   // X,Y value (pwmValue = (uint)((y0+y*a+.5)*PWM_VALUES_LIMIT)
  double c, s;   // cosine and sinus of the step angle 
  double a;      // amplitude of variation (constant when a = 0)
  double dy;     // step size for triangular variation
} genParams_t;

extern volatile genParams_t newParams[NCHAN]; // new parameters set by main thread
extern volatile uint32_t newParamFlags;       // bit set by main thread for each new params
extern atomic_flag newParamsLock;             // lock protecting newParams access
extern volatile double frequencyMean;         // mean frequency with exponentialy decaying weighting
extern volatile double frequencyVariance;     // frequency variance with exponentialy decaying weighting

void* generator(void *);

#endif // GENERATOR_H