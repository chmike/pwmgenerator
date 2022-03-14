#define _GNU_SOURCE
#include "thread.h"
#include "print.h"

#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <sched.h>
#include <errno.h>
#include <string.h>

#define handleError(err, msg) do { errno = err; printErr("%s: %s\n", msg, strerror(errno)); return -1; } while (0)

int startThread(void*(*threadFunction)(void*)) {
  int err;
  pthread_t thid;
  pthread_attr_t attr;
  if ((err = pthread_attr_init(&attr)) != 0)
   handleError(err, "pthread_attr_init");
  if ((err = pthread_create(&thid, &attr, threadFunction, NULL)) != 0)
    handleError(err, "pthread_create");
  if ((err = pthread_attr_destroy(&attr)) != 0)
    handleError(err, "pthread_attr_destroy");
  return 0;
}


// Check affinity: https://unix.stackexchange.com/questions/425065/linux-how-to-know-which-processes-are-pinned-to-which-core
int startPinnedThread(int coreID, void*(*threadFunction)(void*)) {
  pthread_t thid;
  pthread_attr_t attr;
  struct sched_param param;
  int err;
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(coreID, &cpuset);
  param.sched_priority = 99;

  if ((err = pthread_attr_init(&attr)) != 0)
   handleError(err, "pthread_attr_init");
  if ((err = pthread_attr_setaffinity_np(&attr, sizeof(cpuset), &cpuset)) != 0)
    handleError(err, "pthread_attr_setaffinity_np");
  if ((err = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)) != 0)
    handleError(err, "pthread_attr_setinheritsched");
  if ((err = pthread_attr_setschedpolicy(&attr, SCHED_FIFO)) != 0)
    handleError(err, "pthread_attr_setschedpolicy");
  if ((err = pthread_attr_setschedparam(&attr, &param)) != 0)
    handleError(err, "pthread_attr_setschedparam");
  if ((err = pthread_create(&thid, &attr, threadFunction, NULL)) != 0)
    handleError(err, "pthread_create");
  if ((err = pthread_attr_destroy(&attr)) != 0)
    handleError(err, "pthread_attr_destroy");
  return 0;
}


int printThreadSched(const char *msg) {
  int policy, err;
  struct sched_param param;
  if ((err = pthread_getschedparam(pthread_self(), &policy, &param)) != 0)
    handleError(err, "pthread_getschedparam");
  // print("%s: policy=%s priority=%d\n", msg,
  //         (policy == SCHED_FIFO)  ? "SCHED_FIFO" :
  //         (policy == SCHED_RR)    ? "SCHED_RR" :
  //         (policy == SCHED_OTHER) ? "SCHED_OTHER" :
  //         "???",
  //         param.sched_priority);
  return 0;
}

