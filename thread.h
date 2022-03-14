#ifndef THREAD_H
#define THREAD_H

int startThread(void*(*threadFunction)(void*));
int startPinnedThread(int coreID, void*(*thread)(void*));
int printThreadSched(const char *msg);

#endif // SOFT_PWM_H