#include <err.h>
#include <stdio.h>
#include <sndio.h>
#include <stdint.h>

#define RATE 48000
#define RES 48
#define BUFSIZE (RATE / RES)

int main() {
    uint8_t buf[RATE];
    struct sio_hdl *s;
    struct sio_par p;
    int n, x = 0;
   
    s = sio_open(SIO_DEVANY, SIO_REC, 0);
    sio_initpar(&p);
    p.bits = 8;
    p.bps = 1;
    p.rchan = 1;
    p.rate = RATE;
    p.bufsz = BUFSIZE;
    p.appbufsz = BUFSIZE;
    sio_setpar(s, &p);
    sio_start(s);
    sio_setvol(s, 0);
    while (1) {
        sio_read(s, buf, BUFSIZE);
        for (n = 0 ; n < BUFSIZE ; n++) {
            if (buf[n] < 127 && buf[n] > 5) {
                printf("%d\n", x++);
                break;
            }
        }
    }
    sio_stop(s);
    sio_close(s);
    return 0;
}
