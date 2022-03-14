#define _GNU_SOURCE
#include "print.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void printTime(FILE* f) {
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    struct tm* tm_info = localtime(&t.tv_sec);
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y%m%d %H:%M:%S", tm_info);
    fprintf(f, "%s.%03ld ", buffer,  t.tv_nsec/1000000);
}

void print(const char *format, ...) {
  pthread_mutex_lock(&mutex);
  printTime(stdout);
  va_list argp;
  va_start(argp, format);
  vprintf(format, argp);
  fflush(stdout);
  pthread_mutex_unlock(&mutex);
}

void printErr(const char *format, ...) {
  pthread_mutex_lock(&mutex);
  printTime(stderr);
  va_list argp;
  va_start(argp, format);
  vfprintf(stderr, format, argp);
  fflush(stderr);
  pthread_mutex_unlock(&mutex);
}
