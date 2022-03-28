#include <err.h>
#include <math.h>
#include <stdio.h>
#include <sndio.h>
#include <stdint.h>
#include <stdlib.h>

#define BITS 8
#define CHAN 2
#define RATE 48000
#define RES 250
#define WINDOW (RATE / RES)
#define BUFSIZE (WINDOW * CHAN)
#define SHIFT 16
#define RESOLUTION 250

typedef struct sio_hdl SioHdl;
typedef struct sio_par SioPar;

typedef struct AvgBuffer {

/* Reads a buffer of audio data according to sndio's recommended settings, but
 * keeps track of averages according to the user's preferred resolution. */ 

  uint32_t        avgL;
  uint32_t        avgR;
  uint32_t        bufsz;
  uint32_t        countdown;
  int32_t         countdownL;
  int32_t         countdownR;
  uint32_t        cutoff;
  uint32_t        tillBang;
  uint32_t        window;
  uint32_t        windowChan;
  SioHdl        * audio;
  int8_t        * data;
} AvgBuffer;

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
  AvgBuffer               ab;
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

void makeAudioN(SioHdl **s, SioPar *p) {
 *s = sio_open(SIO_DEVANY, SIO_REC, 0);
 if (*s == NULL) { errx(1, "couldn't open sound."); }
 sio_initpar(p);
 p->bits = BITS;
 p->sig = 1;
 p->rchan = CHAN;
 if (!sio_setpar(*s, p)) { errx(1, "couldn't apply sound settings"); }
 if (!sio_getpar(*s, p)) { errx(1, "couldn't get sound settings");   }
 if (p->rchan != CHAN)   { errx(1, "expected stereo input");         }
}

Buffer makeBufferN(char *cu, char *mtl, char *ml, char *mr, SioPar *p) {
  int samples = 0;
  Buffer b = {0};
  int cutoff = atoi(cu);
  if (cutoff < 0 || cutoff > 255) { errx(1, "<cutoff> must be [0,255]"); }
  b.ab.cutoff = (uint8_t)cutoff;
  b.muteLength = (uint32_t)atoi(mtl);
  if (*ml == '-') { b.msgL = NULL; } else { b.msgL = ml; }
  if (*mr == '-') { b.msgR = NULL; } else { b.msgR = mr; }
  makeAudioN(&b.ab.audio, p);
  b.ab.window = p->rate / RESOLUTION;
  samples = b.ab.window;
  samples = samples + p->round -1;
  samples -= samples % p->round;
  b.ab.data = calloc(samples * CHAN, BITS/8);
  b.ab.bufsz = samples * CHAN * (BITS/8);
  b.ab.windowChan = b.ab.window;
  b.ab.window *= CHAN;
  b.ab.tillBang = b.ab.window;
  return b;
}

void avgN(Buffer *b, int n) {
  uint32_t ln = (uint32_t)abs(b->ab.data[n]);
  uint32_t rn = (uint32_t)abs(b->ab.data[n + 1]);
  ln = abs(n); ln = ln * ln * ln * ln; ln >>= SHIFT;
  rn = abs(n); rn = rn * rn * rn * rn; rn >>= SHIFT;
  b->ab.avgL = (b->ab.avgL * (b->ab.windowChan - 1) + ln) / b->ab.windowChan;
  b->ab.avgR = (b->ab.avgR * (b->ab.windowChan - 1) + rn) / b->ab.windowChan;
}

void bangN(Buffer *b) {

  b->countdownL--; b->countdownR--;
  b->ab.tillBang = b->ab.window;
}

void readAudioN(Buffer *b) {

/* Read a buffer full of audio data, keeping a running average for each channel.
 * Every `window` bytes, run the `bang` command. The window and buffer length
 * do not have to align perfectly. */

  int n = 0;
  int read = sio_read(b->ab.audio, b->ab.data, b->ab.bufsz);
  for (; n < read; n += CHAN, b->ab.tillBang -= CHAN) {
    if (b->ab.tillBang == 0) { bangN(b); }
  }
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
  struct sio_par p;
  if (argc != 5) { 
    errx(1, "usage: <cutoff 0-255> <mute length> <left msg> <right msg>");
  }
  sio_initpar(&p);
  b = makeBufferN(argv[1], argv[2], argv[3], argv[4], &p);
  sio_start(b.ab.audio);
  while (1) { readAudioN(&b); }
  sio_stop(b.audio);
  sio_close(b.audio);
  free(b.buffer);
  return 0;
}
