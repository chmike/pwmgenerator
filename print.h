#ifndef PRINT_H
#define PRINT_H

// thread safe and unbuffered print to stdout and stderr.
void print(const char *format, ...);
void printErr(const char *format, ...);

#endif // PRINT_H