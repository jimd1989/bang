#include <err.h>
#include <math.h>
#include <stdio.h>
#include <sndio.h>
#include <stdint.h>
#include <stdlib.h>

#define BITS 8
#define CHAN 2
#define RATE 8000
#define RES 400
#define WINDOW (RATE / RES)
#define BUFSIZE (WINDOW * CHAN)
#define SHIFT 16

typedef struct Buffer {

/* Buffer of incoming audio data, plus heaps of control variables. Master. */

  uint8_t                 cutoff;
  uint32_t                avgL;
  uint32_t                avgR;
  uint32_t                countdownL;
  uint32_t                countdownR;
  uint32_t                muteLength;
  char                  * msgL;
  char                  * msgR;
  struct sio_hdl        * audio;
  int8_t                  buffer[BUFSIZE];
} Buffer;

void makeAudio(struct sio_hdl **s) {

/* Open a connection to sio. Unchecked. */

 struct sio_par p;
 *s = sio_open(SIO_DEVANY, SIO_REC, 0);
 sio_initpar(&p);
 p.bits = BITS;
 p.sig = 1;
 p.rchan = CHAN;
 p.rate = RATE;
 p.bufsz = BUFSIZE;
 p.appbufsz = BUFSIZE;
 sio_setpar(*s, &p);
}

Buffer makeBuffer(char *cu, char *mtl, char *ml, char *mr) {

/* Create Buffer from command line arguments. */

  Buffer b = {0};
  int cutoff = atoi(cu);
  if (cutoff < 0 || cutoff > 255) { errx(1, "<cutoff> must be [0,255]"); }
  b.cutoff = (uint8_t)cutoff;
  b.muteLength = (uint32_t)atoi(mtl);
  if (*ml == '-') { b.msgL = NULL; } else { b.msgL = ml; }
  if (*mr == '-') { b.msgR = NULL; } else { b.msgR = mr; }
  makeAudio(&b.audio);
  return b;
}

void avg(uint32_t *cd, uint32_t *avg, uint32_t n, int readChan) {

/* Calculate running average of audio signal if not muted.
/* Higher amplitudes are weighted more to emphasize pulses. */

  if (*cd) { *cd -= 1; } else { 
    n = abs(n);
    n = n * n * n * n;
    n >>= SHIFT;
    *avg = (*avg * (readChan - 1) + n) / readChan;
  }
}

void bang(Buffer *b) {

/* Fire left/right channel messages if either of them meet the cutoff. */

  if ((b->avgL & 255) >= b->cutoff) {
    printf("%s %u\n", b->msgL, b->avgL & 255); fflush(stdout);
    b->countdownL = WINDOW * b->muteLength; b->avgL = 0;
  }
  if ((b->avgR & 255) >= b->cutoff) {
    printf("%s %u\n", b->msgR, b->avgR & 255); fflush(stdout);
    b->countdownR = WINDOW * b->muteLength; b->avgR = 0;
  }
}

void readAudio(Buffer *b) {

/* Read audio from both channels, calculate averages, print messages. */

  int i = 0;
  int read = sio_read(b->audio, b->buffer, BUFSIZE);
  int readChan = read / 2;
  if (b->msgL != NULL) {
    for (i = 0; i < read; i += 2) {
      avg(&b->countdownL, &b->avgL, (uint32_t)b->buffer[i], readChan);
    }
  }
  if (b->msgR != NULL) {
    for (i = 0; i < read; i += 2) {
      avg(&b->countdownR, &b->avgR, (uint32_t)b->buffer[i + 1], readChan);
    }
  }
  bang(b);
  //printf("%d %d %u %u\n", b->avgL, b->avgR, b->countdownL, b->countdownR);
}


int main(int argc, char **argv) {
  Buffer b = {0};
  if (argc != 5) { 
    errx(1, "usage: <cutoff 0-255> <mute length> <left msg> <right msg>");
  }
  b = makeBuffer(argv[1], argv[2], argv[3], argv[4]);
  sio_start(b.audio);
  while (1) { readAudio(&b); }
  sio_stop(b.audio);
  sio_close(b.audio);
  return 0;
}
