#include <err.h>
#include <stdio.h>
#include <sndio.h>
#include <stdint.h>
#include <stdlib.h>

#define RATE 48000
#define RES 5
#define BUFSIZE (RATE / RES)

// called with: bang cutoff div message
int main(int argc, char **argv) {
    uint8_t buf[BUFSIZE] = {0};
    struct sio_hdl *s;
    struct sio_par p;
    uint8_t cutoff = 0;
    unsigned int n, sum, read, skip, div = 0;
    char *msg = NULL;

    if (argc != 4) {
        errx(0, "required args: cutoff 0-255, clock divide, message");
    }
    cutoff = (uint8_t)atoi(argv[1]);
    div = atoi(argv[2]);
    msg = argv[3];
    s = sio_open(SIO_DEVANY, SIO_REC, 0);
    sio_initpar(&p);
    p.bits = 8;
    p.sig = 0;
    p.bps = 1;
    p.rchan = 2;
    p.rate = RATE;
    p.bufsz = BUFSIZE;
    p.appbufsz = BUFSIZE;
    sio_setpar(s, &p);
    sio_start(s);
    while (1) {
        read = sio_read(s, buf, BUFSIZE);
        if (!skip) {
            for (n = 0, sum = 0; n < read ; n += 2) {
                sum += buf[n];
            }
            if ((sum / read / 2) > cutoff) {
                printf("%s\n", msg);
            }
        }
        skip = (skip + 1) % div;
    }
    sio_stop(s);
    sio_close(s);
    return 0;
}
