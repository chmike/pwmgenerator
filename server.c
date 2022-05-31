#include "server.h"
#include "command.h"
#include "thread.h"
#include "hexdump.h"
#include "generator.h"
#include "print.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/time.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <byteswap.h>
#include <stdarg.h>
#include <errno.h>

atomic_flag isConnected = ATOMIC_FLAG_INIT; 
conn_t newConn, conn;

// resets the connection connection
void reset(conn_t *c) {
  c->fd = -1;
  c->len = 0;
  c->beg = c->end = 0;
  c->addrStr[0] = '\0';
}

// setTimeOut for reading
int setTimeOut(conn_t *c,int ms) {
  // print("debug: setTimeOut: fd=%d ms=%d\n", c->fd, ms);
  struct timeval tv;
  tv.tv_sec = ms/1000;
  tv.tv_usec = 1000*(ms%1000);
  return setsockopt(c->fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
}

// recvReq reads next text line ending with \n. Returns the string
// length if positive, 0 if the connection has been closed, -1 in
// case of read error, -2 in case the buffer would overflow. A 
// return value <= 0 is a fatal error. The connection should be 
// closed and the buffer reset.
int recvReq(conn_t *c) {
  // pop previous request if any
  if(c->beg != 0) {
    c->end -= c->beg;
    if(c->end > 0)
      memmove(c->req, c->req+c->beg, c->end);
    c->beg = 0;
  }
  // check if there is already a full line in buffer
  while(c->beg < c->end && c->req[c->beg] != '\n')
    c->beg++;
  if(c->beg < c->end) {
    // we found a \n at indice c->beg, include also the \n
    c->beg++;
    //print("debug: received request:\n");
    //hexdump(c->req, c->beg-c->req);
    return c->beg;
  }
  bool start = c->beg == 0;
  while(1) {
    if(c->end == BUFFER_SIZE)
      return -2;
    if(start)
      // no timout, wait forever
      start = false;
    else
      setTimeOut(c, 500);
    ssize_t n = read(c->fd, c->req+c->end, BUFFER_SIZE - c->end);
    if(n <= 0) {
      if(n == -1) {
        //print("debug: recvReq: read error %d\n", errno);
        printErr("recvReq error: %s\n", strerror(errno));
      }
      return n;
    }
    setTimeOut(c, 0);
    c->end += n;
    while(c->beg < c->end && c->req[c->beg] != '\n')
      c->beg++;
    if(c->beg < c->end) {
      // we found a \n at indice c->beg, include also the \n
      c->beg++;
      // print("received request:\n");
      // hexdump(c->req, c->beg-c->req);
      return c->beg;
    }
  }
}

// sendRspBuf sends the first len bytes of rspBuf and returns
// len, 0 or -1. When not len, the connection must be closed.
// Appends a \n if there is none.
int sendRspBuf(conn_t *c) {
  if(c->len <= 0) {
    printErr("sendRspBuf: response buffer is empty (%d)\n", c->len);
    return -2;
  }
  // append \n if missing
  if(c->len == 0 || c->rsp[c->len-1] != '\n') {
    if(c->len >= BUFFER_SIZE-2) {
       return -3;
    }
    strcpy(c->rsp+c->len++, "\n");
  }
  // print("debug: sendRspBuf: \n");
  // hexdump(c->rsp, c->len);
  char *p = c->rsp;
  while(c->len > 0) {
    ssize_t n = write(c->fd, p, c->len);
    if(n <= 0) {
      if(n == -1)
        printErr("sendRsp error: %s\n", strerror(errno));
      return n;
    }
    p += n;
    c->len -= n;
  }
  return p - c->rsp;
}

// sendRsp sends the string (ending with '\0'). It appends a '\n' if it is
// missing. Return 0 when the connection is closed, -1 in case of error,
// -2 if the buffer would overflow. Otherwise returns the number of lines
// written. It will be bigger than the input string length when a newline
// had to be appended.
int sendRsp(conn_t *c, const char *format, ...) {
  strcpy(c->rsp, ">");
  c->len = 1;
  va_list argp;
  va_start(argp, format);
  int n = vsnprintf(c->rsp+1, BUFFER_SIZE-1, format, argp);
  if(n >= BUFFER_SIZE-1)
    return -2;
  if(n < 0)
    return n;
  c->len += n;
  return sendRspBuf(c);
}

// sendError sends an error message. Appends '\n' if missing.
int sendError(conn_t *c, const char *format, ...) {
  strcpy(c->rsp, "!");
  c->len = 1;
  va_list argp;
  va_start(argp, format);
  int n = vsnprintf(c->rsp+1, BUFFER_SIZE-1, format, argp);
  if(n >= BUFFER_SIZE-1)
    return -2;
  c->len += n;
  return sendRspBuf(c);
}

// close the current connection.
void closeConn(conn_t *c) {
  if(c->fd >= 0) {
    // print("debug: closeConn: fd=%d\n", c->fd);
    close(c->fd);
  }
  reset(c);
}

// addrToString converts the IPv4 address to string, e.g. "1.2.3.4:1234" or "[1:::]:1234".
int addrToString(struct sockaddr_in *addr, char *buf, size_t maxLen) {
  strncpy(buf, "???", maxLen);
  char addrStr[256];
  if(inet_ntop(AF_INET, &addr->sin_addr, addrStr, sizeof(addrStr)) == NULL)
    return -1;
  char tmp[sizeof(addrStr)+10];
  snprintf(tmp, sizeof(tmp), "%s:%d", addrStr, ntohs(addr->sin_port));
  char *s=tmp, *d=buf;
  while((*d++ = *s++));
  return 0;
}

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
int serve(unsigned short port) {
  if(port == 0) {
    fprintf(stderr,"serve error: port is 0\n");
    return 1;
  }
  newConn.fd = -1;
  int listenFD = socket(AF_INET, SOCK_STREAM, 0);
  if(listenFD < 0) {
    printErr("serve error: socket: %s\n", strerror(errno));
    return -1;
  }
  struct sockaddr_in servAddr, cliAddr;
  bzero((char *) &servAddr, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = INADDR_ANY;
  servAddr.sin_port = htons(port);
  if(bind(listenFD, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
    printErr("serve error: bind: %s\n", strerror(errno));
    close(listenFD);
    return -1;
  }
  print("server info: starting and listening on port %d\n", port);
  listen(listenFD, 5);
  socklen_t cliLen = sizeof(cliAddr);
  int res;
  while(1) {
    closeConn(&newConn);
    newConn.fd = accept(listenFD, (struct sockaddr *) &cliAddr, &cliLen);
    if(newConn.fd < 0) {
       printErr("serve warning: accept: %s\n", strerror(errno));
      continue;
    }
    if(addrToString(&cliAddr, newConn.addrStr, sizeof(newConn.addrStr)) != 0) {
      printErr("serve warning: invalid client address\n");
      continue;
    }
    int one = 1;
    if(setsockopt(newConn.fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) != 0){
      printErr("serve warning: failed setting TCP_NODELAY\n");
    }   
    if(setTimeOut(&newConn, 10000) != 0) {
      printErr("serve warning: set timeout: %s\n", strerror(errno));
      continue;
    }
    if((res = recvReq(&newConn)) <= 0) {
      printErr("serve warning: reject invalid connection from %s\n", newConn.addrStr);
      continue;
    }
    if(res != 5 || memcmp(newConn.req, "PWM0\n", 5) != 0) {
      printErr("serve warning: expected \"PWM0\\n\", reject connection from %s\n", newConn.addrStr);
      continue;
    }
    if(!atomic_flag_test_and_set(&isConnected)) {
      // we are available, start command handler
      if((res = sendRsp(&newConn, "HELO %s %dbits\n", version, BITS_RESOLUTION)) <=0) {
        printErr("serve warning: failed replying to \"PWM0\\n\", reject connection from %s\n", newConn.addrStr);
        atomic_flag_clear(&isConnected);
        continue;
      }
      // print("server info: accept connection from %s\n", newConn.addrStr);
      reset(&conn);
      conn.fd = newConn.fd;
      newConn.fd = -1;
      // print("debug: serve: fd=%d\n", conn.fd);
      strcpy(conn.addrStr, newConn.addrStr);
      startThread(&commandHandler);
      continue;
    }
    // we are busy, return notification and close connection
    printErr("serve warning: busy, reject connection from %s\n", newConn.addrStr);
    sendError(&newConn, "busy with %s", newConn.addrStr);
  }
  return -1;
}
