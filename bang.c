#include <err.h>
#include <math.h>
#include <stdio.h>
#include <sndio.h>
#include <stdint.h>
#include <stdlib.h>

/* TODO: 
 * getopts
 * flag for passing velocity along with message
 * change cutoff etc at runtime
 * per channel cutoff
 * measure pulse length
 * */

#define BITS 8          /* Bit depth of audio input                 */
#define CHAN 2          /* Number of channels of audio input        */
#define SIGNED 1        /* Read samples with signs                  */
#define RATE 48000      /* Sampling rate of audio input             */
#define SHIFT 16        /* Bit shift used to emphasize peak samples */
#define RESOLUTION 2000 /* Sampling windows per second              */

typedef struct sio_hdl SioHdl;
typedef struct sio_par SioPar;

typedef struct Buffer {

/* Reads a buffer of audio data according to sndio's recommended settings, but
 * keeps track of averages according to the user's preferred resolution. */ 

  uint32_t        avgL;         /* Left channel samples running average      */
  uint32_t        avgR;         /* Right channel samples running average     */
  uint32_t        bufsz;        /* Audio buffer size in bytes                */
  uint32_t        countdown;    /* RO: how many sampling windows to mute     */
  int32_t         countdownL;   /* Left channel mute window countdown        */
  int32_t         countdownR;   /* Right channel mute window countdown       */
  uint32_t        cutoff;       /* RO: amplitude limit for bang              */
  uint32_t        tillBang;     /* How many samples until next bang check    */
  uint32_t        window;       /* RO: size of a sampling window in bytes    */
  uint32_t        windowChan;   /* RO: half size of sampling window in bytes */
  char          * msgL;         /* RO: message to fire on left channel bang  */
  char          * msgR;         /* RO: message to fire of right channel bang */
  SioHdl        * audio;        /* RO: audio stream                          */
  int8_t        * data;         /* Buffer of audio data, `bufsz` bytes long  */
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

Buffer makeBuffer(char *cu, char *mtl, char *ml, char *mr, SioPar *p) {

/* Make Buffer from command line arguments and hardware settings. */

  int samples = 0;
  Buffer b = {0};
  int cutoff = atoi(cu);
  if (cutoff < 0 || cutoff > 255) { errx(1, "<cutoff> must be [0,255]"); }
  b.cutoff = (uint8_t)cutoff;
  b.countdown = (uint32_t)abs(atoi(mtl));
  if (*ml == '-') { b.msgL = NULL; } else { b.msgL = ml; }
  if (*mr == '-') { b.msgR = NULL; } else { b.msgR = mr; }
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
  return b;
}

void avg(Buffer *b, int n) {

/* Add frame `n` (two interleaved stereo bytes) to the running average on each
 * channel. Samples are 8 bit, but absolute value renders them 7 bit. They are
 * cast to 32 bit for headroom during averaging and peak emphasis process. */

  uint32_t ln = (uint32_t)abs(b->data[n]);
  uint32_t rn = (uint32_t)abs(b->data[n + 1]);
  ln = ln * ln * ln * ln; ln >>= SHIFT;
  rn = rn * rn * rn * rn; rn >>= SHIFT;
  b->avgL = (b->avgL * (b->windowChan - 1) + ln) / b->windowChan;
  b->avgR = (b->avgR * (b->windowChan - 1) + rn) / b->windowChan;
}

void bang(Buffer *b) {

/* If a channel is not currently muted and its average exceeds the cutoff, then
 * fire off its message to stdout. */

  b->countdownL--; b->countdownR--;
  /* Debug
  printf("%d %d %d\n", b->avgL, b->countdownL, b->window);
  */
  if (b->msgL != NULL && b->countdownL <= 0 && (b->avgL & 255) >= b->cutoff) {
    printf("%s %d\n", b->msgL); fflush(stdout); b->countdownL = b->countdown;
  }
  if (b->msgR != NULL && b->countdownR <= 0 && (b->avgR & 255) >= b->cutoff) {
    printf("%s %d\n", b->msgR); fflush(stdout); b->countdownR = b->countdown;
  }
  b->tillBang = b->window;
}

void readAudio(Buffer *b) {

/* Read a buffer full of audio data, keeping a running average for each channel.
 * Every `window` bytes, run the `bang` command. The window and buffer length
 * do not have to align perfectly. */

  int n = 0;
  int read = sio_read(b->audio, b->data, b->bufsz);
  for (; n < read; n += CHAN, b->tillBang -= CHAN) {
    if (b->tillBang == 0) { bang(b); }
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
  sio_start(b.audio);
  while (1) { readAudio(&b); }
  sio_stop(b.audio);
  sio_close(b.audio);
  free(b.data);
  return 0;
}
