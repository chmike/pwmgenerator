#ifndef PRINT_H
#define PRINT_H

// print is a thread safe and unbuffered print to stdout.
// A newline is not automatically appended.
void print(const char *format, ...);

// printErr is a thread safe and unbuffered print to stderr.
// A newline is not automatically appended.
void printErr(const char *format, ...);

#endif // PRINT_H