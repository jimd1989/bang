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
  char          * msgL;
  char          * msgR;
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


void makeAudio(SioHdl **s, SioPar *p) {
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

Buffer makeBuffer(char *cu, char *mtl, char *ml, char *mr, SioPar *p) {
  int samples = 0;
  Buffer b = {0};
  int cutoff = atoi(cu);
  if (cutoff < 0 || cutoff > 255) { errx(1, "<cutoff> must be [0,255]"); }
  b.ab.cutoff = (uint8_t)cutoff;
  b.muteLength = (uint32_t)atoi(mtl);
  if (*ml == '-') { b.ab.msgL = NULL; } else { b.ab.msgL = ml; }
  if (*mr == '-') { b.ab.msgR = NULL; } else { b.ab.msgR = mr; }
  makeAudio(&b.ab.audio, p);
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

void avg(Buffer *b, int n) {
  uint32_t ln = (uint32_t)abs(b->ab.data[n]);
  uint32_t rn = (uint32_t)abs(b->ab.data[n + 1]);
  ln = abs(n); ln = ln * ln * ln * ln; ln >>= SHIFT;
  rn = abs(n); rn = rn * rn * rn * rn; rn >>= SHIFT;
  b->ab.avgL = (b->ab.avgL * (b->ab.windowChan - 1) + ln) / b->ab.windowChan;
  b->ab.avgR = (b->ab.avgR * (b->ab.windowChan - 1) + rn) / b->ab.windowChan;
}

void bang(Buffer *b) {
  b->ab.countdownL--; b->ab.countdownR--;
  if (b->ab.countdownL <= 0 && (b->ab.avgL & 255) >= b->ab.cutoff) {
    printf("%s %d\n", b->ab.msgL, b->ab.avgL); fflush(stdout); b->ab.countdownL = b->ab.countdown;
  }
  b->ab.tillBang = b->ab.window;
}

void readAudio(Buffer *b) {

/* Read a buffer full of audio data, keeping a running average for each channel.
 * Every `window` bytes, run the `bang` command. The window and buffer length
 * do not have to align perfectly. */

  int n = 0;
  int read = sio_read(b->ab.audio, b->ab.data, b->ab.bufsz);
  for (; n < read; n += CHAN, b->ab.tillBang -= CHAN) {
    if (b->ab.tillBang == 0) { bang(b); }
    avg(b, n);
  }
}

int main(int argc, char **argv) {
  Buffer b = {0};
  struct sio_par p;
  if (argc != 5) { 
    errx(1, "usage: <cutoff 0-255> <mute length> <left msg> <right msg>");
  }
  sio_initpar(&p);
  b = makeBuffer(argv[1], argv[2], argv[3], argv[4], &p);
  sio_start(b.ab.audio);
  while (1) { readAudio(&b); }
  sio_stop(b.audio);
  sio_close(b.audio);
  free(b.buffer);
  return 0;
}
