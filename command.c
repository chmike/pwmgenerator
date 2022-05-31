#include "command.h"
#include "server.h"
#include "gpio.h" 
#include "generator.h"
#include "hexdump.h"
#include "print.h"

#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <errno.h>


char *version = "v0.1.1";

#define PI 3.14159265358979323846

#define CMD_GET_STATUS 0
#define CMD_SET_PARAMS 1

// cmdParams hols the user provided parameters. The type is specify the 
// type of generation: 0 = cst, 1 = sinusoidal, 2 = triangular. When cst,
// only the average must be provided, all other values mist be 0.
// When sinusoidal, the amplitude must be different from 0, and 
// amplitude+average <= 1 and average-amplitude >= 0. The start is a 
// value in the range [0,1] and specifies the advance in the period
// at the start of generation.
// When triangular, it is the same as for sinusoidal except that the
// generated signal will be triangular.

#define CST_PARAM 0
#define SIN_PARAM 1
#define TRI_PARAM 2

typedef struct {
  uint8_t type;     // type of generation: 0 = cst, 1 = sinusoidal, 2 = triangular
  double average;   // value in the range [0,1]
  double amplitude; // amplitude of variation 
  double period;    // duration in seconds of one period
  double start;     // start in percentage of period range [0,1]
} cmdParams_t;

cmdParams_t cmdParams[NCHAN];
char errStr[1024];


// checkParams checks the validity of the given params and return NULL
// if everything is OK. It returns a pointer to errStr that has
// been filled with an error message to return if a field is invalid.
char* checkParams(int ch, cmdParams_t *p) {
  double val;
  switch(p->type) {
  case CST_PARAM:
    if(p->average < 0 || p->average > 1) {
      snprintf(errStr, sizeof(errStr), "channel[%d]: expect average of cst function to be in the range [0,1], got %f", ch, p->average);
      return errStr;
    }
    if(p->amplitude != 0) {
      snprintf(errStr, sizeof(errStr), "channel[%d]: expect amplitude of constant function to be 0", ch);
      return errStr;
    }
    if(p->period != 0) {
      snprintf(errStr, sizeof(errStr), "channel[%d]: expect period of constant function to be 0", ch);
      return errStr;
    }
    if(p->start != 0) {
      snprintf(errStr, sizeof(errStr), "channel[%d]: expect start of constant function to be 0", ch);
      return errStr;
    }
    break;
  case SIN_PARAM:
    if(p->average < 0 || p->average > 1) {
      snprintf(errStr, sizeof(errStr), "channel[%d]: expect average of sinusoidal function to be in the range [0,1], got %f", ch, p->average);
      return errStr;
    }
    if(p->amplitude == 0) {
      snprintf(errStr, sizeof(errStr), "channel[%d]: expect amplitude of sinusoidal function to be different of 0", ch);
      return errStr;
    }
    val = p->average + p->amplitude;
    if(val > 1) {
      snprintf(errStr, sizeof(errStr), "channel[%d]: expect average+amplitude of sinusoidal function to be <= 1, got %f", ch, val);
      return errStr;
    }
    val = p->average - p->amplitude;
    if(val < 0) {
      snprintf(errStr, sizeof(errStr), "channel[%d]: expect average-amplitude of sinusoidal function to be >= 0, got %f", ch, val);
      return errStr;
    }
    if(p->period <= 0) {
      snprintf(errStr, sizeof(errStr), "channel[%d]: expect period of sinusoidal function to be > 0, got %f", ch, p->period);
      return errStr;
    }
    if(p->start < 0 || p->start >= 1) {
      snprintf(errStr, sizeof(errStr), "channel[%d]: expect start of sinusoidal function to be in the range [0,1[, got %f", ch, p->start);
      return errStr;
    }
    break;
  case TRI_PARAM:
    if(p->average < 0 || p->average > 1) {
      snprintf(errStr, sizeof(errStr), "channel[%d]: expect average of triangular function to be in the range [0,1], got %f", ch, p->average);
      return errStr;
    }
    if(p->amplitude == 0) {
      snprintf(errStr, sizeof(errStr), "channel[%d]: expect amplitude of triangular function to be different of 0", ch);
      return errStr;
    }
    val = p->average + p->amplitude;
    if(val > 1) {
      snprintf(errStr, sizeof(errStr), "channel[%d]: expect average+amplitude of triangular function to be <= 1, got %f", ch, val);
      return errStr;
    }
    val = p->average - p->amplitude;
    if(val < 0) {
      snprintf(errStr, sizeof(errStr), "channel[%d]: expect average-amplitude of triangular function to be >= 0, got %f", ch, val);
      return errStr;
    }
    if(p->period <= 0) {
      snprintf(errStr, sizeof(errStr), "channel[%d]: expect period of triangular function to be > 0, got %f", ch, p->period);
      return errStr;
    }
    if(p->start < 0 || p->start >= 1) {
      snprintf(errStr, sizeof(errStr), "channel[%d]: expect start of triangular function to be in the range [0,1[, got %f", ch, p->start);
      return errStr;
    }
    break;
  default:
    snprintf(errStr, sizeof(errStr), "channel[%d]: invalid channel parameter type, got %d", ch, p->type);
    return errStr;
  }
  return NULL;
}

void convertParams(volatile genParams_t *g, cmdParams_t *p){
  double pulsePerSeconds = frequencyMean, pulsePerPeriod;
  if(pulsePerSeconds == 0)
    pulsePerSeconds = 10156.78; // measure on raspberry PI4
  switch(p->type) {
  case CST_PARAM:
    g->a = g->dy = g->x = g->y = g->c = g->s = 0;
    g->y0 = p->average;
    break;
  case SIN_PARAM:
    g->a = g->dy = 0;
    pulsePerPeriod = pulsePerSeconds*p->period;
    g->y0 = p->average;
    double angleStep = 2*PI/pulsePerPeriod;
    g->c = cos(angleStep);
    g->s = sin(angleStep);
    double angle0 = 2*PI*p->start;
    g->x = cos(angle0)*p->amplitude;
    g->y = sin(angle0)*p->amplitude;
    break;
  case TRI_PARAM:
    g->x = g->c = g->s = 0;
    pulsePerPeriod = pulsePerSeconds*p->period;
    g->y0 = p->average;
    g->a = p->amplitude;
    g->dy = p->amplitude*4/pulsePerPeriod;
    if(p->start < 0.25) {
      g->y = g->dy*pulsePerPeriod*p->start;
    } else if(p->start < 0.75) {
      g->dy = -g->dy;
      g->y = g->a - g->dy*(p->start-0.25)*pulsePerPeriod;
    } else {
      g->y = -g->a - g->dy*(p->start-0.75)*pulsePerPeriod;
    }
    break;
  }
}

const char *TYPE[] = {"CST", "SIN", "TRI"};

// requestGetParams handles a getParams (GPRM) request. It has no
// arguments, 
int requestGetParams(char *beg, char *end) {
  if(beg == end || *beg != '\n')
    return sendError(&conn, "unexpected data after \"GPRM\"");
  char buf[65536], *p = buf;
  ssize_t len = sizeof(buf);
  ssize_t n = snprintf(p, len, "%d", NCHAN);
  if(n >= len) {
      printErr("requestGetParams: output truncated\n");
    return n;
  }
  p += n;
  len -= n;
  for(int ch = 0; ch < NCHAN; ch++) {
    n = snprintf(p, len, ", %d %s %g %g %g %g", ch, TYPE[cmdParams[ch].type], cmdParams[ch].average, cmdParams[ch].amplitude, cmdParams[ch].period, cmdParams[ch].start); 
    if(n >= len) {
      printErr("requestGetParams: output truncated\n");
      return n;
    }
    p += n;
    len -= n;
  }
  //print("debug: sendRsp: %s\n", buf);
  return sendRsp(&conn, buf);
}

int requestSetParams(char *beg, char *end) {
  if(beg == end) {
    return sendError(&conn, "expected arguments to \"SPRM\"");
  }
  end--;
  cmdParams_t newCmdParams[NCHAN];
  bool hasCmdParams[NCHAN];
  bzero(newCmdParams, sizeof(newCmdParams));
  bzero(hasCmdParams, sizeof(hasCmdParams));
  // decode parameters
  int nParams = 0;
  char *p = beg;
  int len = end-beg, consumed;
  int n = sscanf(p, "%d%n", &nParams, &consumed);
  if(n <= 0) {
    printErr("requestSetParams: failed parsing \"%.*s\" (%d)\n", len, beg, n);
    return sendError(&conn, "invalid arguments");
  }
  p += consumed;
  len -= consumed;
  for(int i = 0; i < nParams; i++) {
    int ch = -1;
    char type[4] = "";
    double average = 0, amplitude = 0, period = 0, start = 0;
    n = sscanf(p, ", %d %3s %lg %lg %lg %lg%n", &ch, type, &average, &amplitude, &period, &start, &consumed);
    if(n <= 0) {
      printErr("requestSetParams: failed parsing \"%.*s\" (%d)\n", (int)(end-beg)-1, beg, n);
      return sendError(&conn, "invalid arguments");
    }
    p += consumed;
    len -= consumed;
    if(ch < 0 || ch >= NCHAN) {
      printErr("requestSetParams: invalid channel %d\n", ch);
      return sendError(&conn, "channel number out of range");
    }
    if(strcmp(type, "CST") == 0)
      newCmdParams[ch].type = 0;
    else if(strcmp(type, "SIN") == 0)
      newCmdParams[ch].type = 1;
    else if(strcmp(type, "TRI") == 0)
      newCmdParams[ch].type = 2;
    else {
      printErr("requestSetParams: channel %d assigned invalid type %s\n", ch, type);
      return sendError(&conn, "channel %d assigned invalid type %s", ch, type);
    }
    hasCmdParams[ch] = true;
    newCmdParams[ch].average = average;
    newCmdParams[ch].amplitude = amplitude;
    newCmdParams[ch].period = period;
    newCmdParams[ch].start = start;
  }
  // check parameter validity
  for(int ch = 0; ch < NCHAN; ch++) {
    if(!hasCmdParams[ch])
      continue;
    char *err = checkParams(ch, newCmdParams+ch);
    if(err != NULL) {
      printErr("requestSetParams: error: %s\n", err);
      return sendError(&conn, err);
    }
  }
  // store new command parameters
  for(int ch = 0; ch < NCHAN; ch++)
    if(hasCmdParams[ch])
      cmdParams[ch] = newCmdParams[ch];

  // convert parameters and pass it to generator
  while(atomic_flag_test_and_set(&newParamsLock));
  uint16_t flag = 0;
  for(int ch = 0; ch < NCHAN; ch++) {
    if(!hasCmdParams[ch])
      continue;
    convertParams(newParams+ch, cmdParams+ch);
    flag |= 1 << ch; 
  }
  newParamFlags = flag;
  atomic_flag_clear(&newParamsLock);
  return sendRsp(&conn, "DONE");
}

int requestFrequency(char *beg, char *end) {
  if(beg == end || *beg != '\n')
    return sendError(&conn, "unexpected data after \"FREQ\"\n");
  for(int i = 0; i < 4 && frequencyMean == 0; i++)
    sleep(1);
  return sendRsp(&conn, "%g %g", frequencyMean, sqrt(frequencyVariance));
}

// command handler thread
void* commandHandler(void* dummy) {
  UNUSED(dummy);
  //print("command info: started\n");
  newParamFlags = 0;
  startPinnedThread(3, &generator);
  int res;
  print("start accepting commands from %s\n", conn.addrStr);
  do {
    //print("debug: commandHandler: wait for a request\n");
    if((res = recvReq(&conn)) <= 0)
      break;
    if(res < 5) {
      printErr("command warning: received invalid request \"%.*s\" from %s\n", res, conn.req, conn.addrStr);
      res = sendError(&conn, "invalid request \"%.*s\"", res, conn.req);
      continue;
    }
    char *end = conn.req+res;
    if(memcmp(conn.req, "GPRM", 4) == 0) {
      res = requestGetParams(conn.req+4, end);
    } else if(memcmp(conn.req, "SPRM ", 5) == 0) {
      res = requestSetParams(conn.req+5, end);
    } else if(memcmp(conn.req, "FREQ", 4) == 0) {
      res = requestFrequency(conn.req+4, end);
    } else {
      printErr("command warning: received undefined request \"%.*s\" from %s\n", res, conn.req, conn.addrStr);
      res = sendError(&conn, "undefined request \"%.*s\"", res-1, conn.req);
    }
  } while(res > 0);
  print("stop accepting commands from %s\n", conn.addrStr);
  closeConn(&conn);
  //print("command info: stopping generator...\n");
  while(atomic_flag_test_and_set(&newParamsLock));
  newParamFlags = 1 << NCHAN; // request to stop flag
  atomic_flag_clear(&newParamsLock);
  //print("command info: stopped\n");
  atomic_flag_clear(&isConnected);
  return NULL;
}
