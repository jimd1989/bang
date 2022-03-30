#include <err.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <sndio.h>
#include <stdint.h>
#include <stdlib.h>

/* TODO: 
 * getopts
 * flag for passing velocity along with message
 * change cutoff etc at runtime
 * runtime mute button (start muted?)
 * per channel cutoff
 * measure pulse length
 * CHANGE COMMENTS / DOCS
 * */

#define BITS 8             /* Bit depth of audio input                   */
#define CHAN 2             /* Number of channels of audio input          */
#define SIGNED 1           /* Read samples with signs                    */
#define RATE 48000         /* Sampling rate of audio input               */
#define SHIFT 16           /* Bit shift used to emphasize peak samples   */
#define RESOLUTION 1000    /* Sampling windows per second                */
#define MUTE_CHAR '-'      /* Mute channel for this message              */

typedef struct sio_hdl SioHdl;
typedef struct sio_par SioPar;

typedef struct PulseClock {

/* 1. Keeps a running average off all amplitudes on channel in `avgSample`.
 * 2. Detects pulses (when `avgSample` exceeds `cutoff`).
 * 3. Measures distance between pulses and keeps running average in `avgPeriod`.
 * 4. Prints `msg` every `avgPeriod` sampling windows.
 * This ensures a steady rhythm that cannot be upset by erratic amplitudes. */ 

  bool            pulseOn;         /* Signal is currently pulsing            */
  uint8_t         cutoff;          /* RO: pulse detection cutoff             */
  uint32_t        avgSample;       /* Running average of amplitudes          */
  char          * msg;             /* RO: printed when `periodCountdown` = 0 */
} PulseClock;

typedef struct Buffer {

/* Reads a buffer of audio data according to sndio's recommended settings, but
 * keeps track of averages according to the user's preferred resolution. */ 

  uint32_t        bufsz;        /* Audio buffer size in bytes                */
  uint32_t        tillBang;     /* How many samples until next bang check    */
  uint32_t        window;       /* RO: size of a sampling window in bytes    */
  uint32_t        windowChan;   /* RO: half size of sampling window in bytes */
  char          * msgL;         /* RO: message to fire on left channel bang  */
  char          * msgR;         /* RO: message to fire of right channel bang */
  SioHdl        * audio;        /* RO: audio stream                          */
  int8_t        * data;         /* Buffer of audio data, `bufsz` bytes long  */
  PulseClock      pulseL;       /* Pulse Clock for left channel              */
  PulseClock      pulseR;       /* Pulse Clock for right channel             */
} Buffer;

void makeAudio(SioHdl **s, SioPar *p) {

/* Initialize connection to sndio with proper settings. The hardware may reject 
 * the suggested buffer size. `makeBuffer` will recover from this. */

 *s = sio_open(SIO_DEVANY, SIO_REC, 0);
 if (*s == NULL) { errx(1, "couldn't open sound."); }
 sio_initpar(p);
 p->bits = BITS;
 p->sig = SIGNED;
 p->rchan = CHAN;
 p->appbufsz = RATE / RESOLUTION;
 if (!sio_setpar(*s, p)) { errx(1, "couldn't apply sound settings"); }
 if (!sio_getpar(*s, p)) { errx(1, "couldn't get sound settings");   }
 if (p->rchan != CHAN)   { errx(1, "expected stereo input");         }
}

PulseClock makePulseClock(uint8_t cutoff, char *msg) {

/* Initialize a new Pulse Clock. */

  PulseClock pc = {0};
  pc.cutoff = cutoff;
  pc.msg = msg;
  return pc;
}

Buffer makeBuffer(char *cu, char *mtl, char *ml, char *mr, SioPar *p) {

/* Make Buffer from command line arguments and hardware settings. */

  int samples = 0;
  Buffer b = {0};
  int cutoff = atoi(cu);
  if (cutoff < 0 || cutoff > 255) { errx(1, "<cutoff> must be [0,255]"); }
  makeAudio(&b.audio, p);
  b.window = p->rate / RESOLUTION;
  samples = b.window;
  samples = samples + p->round -1;
  samples -= samples % p->round;
  b.data = calloc(samples * CHAN, BITS/8);
  b.bufsz = samples * CHAN * (BITS/8);
  b.windowChan = b.window;
  b.window *= CHAN;
  b.tillBang = b.window;
  b.pulseL = makePulseClock((uint8_t)cutoff, ml);
  b.pulseR = makePulseClock((uint8_t)cutoff, mr);
  return b;
}

uint32_t emphasize(int8_t amplitude) {

/* Emphasize higher amplitudes over lower ones. Output is expected to be
 * [0,255] if input is provided at a reasonable recording volume, but this is
 * not enforced to avoid integer wrapping. Please adjust your sndio settings if
 * amplitudes are clipping in this manner. */

  uint32_t n = (uint32_t)abs(amplitude);
  n = n * n * n * n;
  return n >> SHIFT;
}

uint32_t avg(uint32_t a, uint32_t n, uint32_t d) {

/* Recalculate running average `a` over `d` samples with `n`. */

  return (a * (d - 1) + n) / d;
}

void pulseCheck(PulseClock *pc) {

/* 1. Measures the distance between each pulse and averages them as `avgPeriod`.
 * 2. Fires off `msg` after `avgPeriod` sampling windows have passed.
 * Keeping messaging and pulse detection separate ensures a steady beat and 
 * reduces the number of user-specified parameters needed to fine tune the
 * sync, but the rhythm will be inaccurate until the average "warms up." */

  if (*pc->msg == MUTE_CHAR) { return; }
  if (pc->pulseOn && pc->avgSample >= pc->cutoff) { 
    return;
  } else if (pc->pulseOn && pc->avgSample < pc->cutoff) {
    pc->pulseOn = false;
  } else if (pc->avgSample >= pc->cutoff) {
    printf("%s\n", pc->msg); fflush(stdout); pc->pulseOn = true;
  }
}

void readAudio(Buffer *b) {

/* Read a buffer full of audio data, keeping a running average for each channel.
 * Every `window` bytes, run the `bang` command. The window and buffer length
 * do not have to align perfectly. */

  int n = 0;
  PulseClock *pc = NULL;
  int read = sio_read(b->audio, b->data, b->bufsz);
  for (; n < read; n += CHAN, b->tillBang -= CHAN) {
    if (b->tillBang == 0) {
      pulseCheck(&b->pulseL);
      pulseCheck(&b->pulseR);
      b->tillBang = b->window;
    }
    pc = &b->pulseL;
    pc->avgSample = avg(pc->avgSample, emphasize(b->data[n]), b->windowChan);
    pc = &b->pulseR;
    pc->avgSample = avg(pc->avgSample, emphasize(b->data[n+1]), b->windowChan);
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
  sio_start(b.audio);
  while (1) { readAudio(&b); }
  sio_stop(b.audio);
  sio_close(b.audio);
  free(b.data);
  return 0;
}
