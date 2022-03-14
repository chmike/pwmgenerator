#ifndef SERVER_H
#define SERVER_H

#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>


#define BUFFER_SIZE 1024

typedef struct {
  int fd, len;
  char rsp[BUFFER_SIZE];
  char req[BUFFER_SIZE];
  char *beg, *end;
  char addrStr[256];
} conn_t;

extern conn_t conn;             // active connection info
extern atomic_flag isConnected; // set when connection is active

// resets the connection connection
void reset(conn_t *c);

// setTimeOut for reading
int setTimeOut(conn_t *c,int ms);

// recvReq reads next text line ending with \n. Returns the string
// length if positive, 0 if the connection has been closed, -1 in
// case of read error, -2 in case the buffer would overflow. A 
// return value <= 0 is a fatal error. The connection should be 
// closed and the buffer reset.
int recvReq(conn_t *c);

// sendRsp sends the string (ending with '\0'). It appends a '\n' if it is
// missing. Return 0 when the connection is closed, -1 in case of error,
// -2 if the buffer would overflow. Otherwise returns the number of lines
// written. It will be bigger than the input string length when a newline
// had to be appended.
int sendRsp(conn_t *c, const char *format, ...);

// sendError is like sendRsp, but sends an error message instead.
int sendError(conn_t *c, const char *format, ...);

// close the current connection.
void closeConn(conn_t *c);

// The server function blocks forever handling connection requests. 
// Incomming connections requests are silently discarded if they don't 
// send a valid request or timeout. If the request is valid and there
// is no active connection, a thread is spawn to handle te connection.
// Otherwise the server returns an error response giving the address
// of the remote connected host.
//
// The server returns immediately if it failed to start listening. The 
// program must then exit with an error code. 
//
// The port value must be bigger than 0 and an unused port.
int serve(unsigned short port);

#endif // SERVER_H