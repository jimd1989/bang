#include <err.h>
#include <math.h>
#include <stdio.h>
#include <sndio.h>
#include <stdint.h>
#include <stdlib.h>

#define BITS 8
#define CHAN 2
#define RATE 24000
#define RES 286
#define WINDOW (RATE / RES)
#define BUFSIZE (WINDOW * CHAN)
#define SHIFT 16
#define RESOLUTION 512

typedef struct Buffer {

/* Buffer of incoming audio data, plus heaps of control variables. Master. */

  uint8_t                 cutoff;
  uint32_t                avgL;
  uint32_t                avgR;
  int32_t                 countdownL;
  int32_t                 countdownR;
  uint32_t                muteLength;
  char                  * msgL;
  char                  * msgR;
  struct sio_hdl        * audio;
  int8_t                  buffer[BUFSIZE];
  int8_t                * bufferN;
  int32_t                 bufsz;
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
 errx(1, "break");
}

void makeAudioN(struct sio_hdl **s, struct sio_par *p) {
 *s = sio_open(SIO_DEVANY, SIO_REC, 0);
 if (*s == NULL) { errx(1, "couldn't open sound."); }
 sio_initpar(p);
 p->bits = BITS;
 p->sig = 1;
 p->rchan = CHAN;
 if (!sio_setpar(*s, p)) { errx(1, "couldn't apply sound settings"); }
 if (!sio_getpar(*s, p)) { errx(1, "couldn't get sound settings"); }
}

Buffer makeBufferN(char *cu, char *mtl, char *ml, char *mr, struct sio_par *p) {
  int samples = 0;
  Buffer b = {0};
  int cutoff = atoi(cu);
  if (cutoff < 0 || cutoff > 255) { errx(1, "<cutoff> must be [0,255]"); }
  b.cutoff = (uint8_t)cutoff;
  b.muteLength = (uint32_t)atoi(mtl);
  if (*ml == '-') { b.msgL = NULL; } else { b.msgL = ml; }
  if (*mr == '-') { b.msgR = NULL; } else { b.msgR = mr; }
  makeAudioN(&b.audio, p);
  samples = p->rate / RESOLUTION;
  samples -= samples % p->round - p->round;
  b.bufferN = calloc(samples * p->rchan, sizeof(int8_t));
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

void avg(uint32_t *avg, uint32_t n, int readChan) {

/* Calculate running average of audio signal. 
 * Higher amplitudes are weighted more to emphasize pulses. */

  n = abs(n);
  n = n * n * n * n;
  n >>= SHIFT;
  *avg = (*avg * (readChan - 1) + n) / readChan;
}

void bang(Buffer *b, int readChan) {

/* Fire left/right channel messages if either of them meet the cutoff. */

  b->countdownL--; b->countdownR--;
  if (b->countdownL <= 0 && (b->avgL & 255) >= b->cutoff) {
    printf("%s %d %d %d\n", b->msgL, b->avgL, b->countdownL, readChan); fflush(stdout); b->countdownL = b->muteLength;
  }
  if (b->countdownR <= 0 && (b->avgR & 255) >= b->cutoff) {
    printf("%s %d %d\n", b->msgR, b->avgR, b->countdownR); fflush(stdout); b->countdownR = b->muteLength;
  }
}

void readAudio(Buffer *b) {

/* Read audio from both channels, calculate averages, print messages. */

  int i = 0;
  int read = sio_read(b->audio, b->buffer, BUFSIZE);
  int readChan = read / 2;
  if (b->msgL != NULL) {
    for (i = 0; i < read; i += 2) {
      avg(&b->avgL, (uint32_t)b->buffer[i], readChan);
    }
  }
  if (b->msgR != NULL) {
    for (i = 0; i < read; i += 2) {
      avg(&b->avgR, (uint32_t)b->buffer[i + 1], readChan);
    }
  }
  /*
  bang(b, readChan);
  */
  printf("%d %d %u %u %d\n", b->avgL, b->avgR, b->countdownL, b->countdownR, readChan);
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
