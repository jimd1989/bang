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
#define SHIFT 24

typedef struct Buffer {
  uint8_t                 cutoff;
  uint32_t                avg_l;
  uint32_t                avg_r;
  uint32_t                countdown_l;
  uint32_t                countdown_r;
  uint32_t                mute_length;
  char                  * msg_l;
  char                  * msg_r;
  struct sio_hdl        * audio;
  int8_t                  buffer[BUFSIZE];
} Buffer;

void makeAudio(struct sio_hdl **s) {
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
  Buffer b = {0};
  int cutoff = atoi(cu);
  if (cutoff < 0 || cutoff > 255) { errx(1, "<cutoff> must be [0,255]"); }
  b.cutoff = (uint8_t)cutoff;
  b.mute_length = (uint32_t)atoi(mtl);
  if (*ml == '-') { b.msg_l = NULL; } else { b.msg_l = ml; }
  if (*mr == '-') { b.msg_r = NULL; } else { b.msg_r = mr; }
  makeAudio(&b.audio);
  return b;
}

void avg(uint32_t *cd, uint32_t *avg, uint32_t n, int readChan) {
  if (*cd) { 
    *cd -= 1; 
  } else { 
    n = abs(n);
    n = n * n * n * n;
    n >>= 16;
    *avg = (*avg * (readChan - 1) + n) / readChan;
  }
}

void bang(Buffer *b) {
  if ((b->avg_l & 255) >= b->cutoff) {
    printf("%s %u\n", b->msg_l, b->avg_l & 255); fflush(stdout);
    b->countdown_l = WINDOW * b->mute_length; b->avg_l = 0;
  }
  if ((b->avg_r & 255) >= b->cutoff) {
    printf("%s %u\n", b->msg_r, b->avg_r & 255); fflush(stdout);
    b->countdown_r = WINDOW * b->mute_length; b->avg_r = 0;
  }
}

void readAudio(Buffer *b) {
  int i = 0;
  int read = sio_read(b->audio, b->buffer, BUFSIZE);
  int readChan = read / 2;
  if (b->msg_l != NULL) {
    for (i = 0; i < read; i += 2) {
      avg(&b->countdown_l, &b->avg_l, (uint32_t)b->buffer[i], readChan);
    }
  }
  if (b->msg_r != NULL) {
    for (i = 0; i < read; i += 2) {
      avg(&b->countdown_r, &b->avg_r, (uint32_t)b->buffer[i + 1], readChan);
    }
  }
  bang(b);
  //printf("%d %d %u %u\n", b->avg_l, b->avg_r, b->countdown_l, b->countdown_r);
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
