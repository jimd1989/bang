#include <err.h>
#include <stdio.h>
#include <sndio.h>
#include <stdint.h>
#include <stdlib.h>

#define RATE 4000
#define RES 50
#define BUFSIZE (RATE / RES)

int main(int argc, char **argv) {
    uint8_t buf[BUFSIZE] = {0};
    struct sio_hdl *s;
    struct sio_par p;
    uint8_t cutoff = 0;
    unsigned int n, read = 0;
    char *msg = NULL;
    float div, inc, sum = 0.0f;
    float dampen = 1.0f;

    if (argc != 4) {
        errx(0, "required args: cutoff 0-255, clock divide, message");
    }
    cutoff = (uint8_t)atoi(argv[1]);
    div = atof(argv[2]);
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
        for (n = 0, sum = 0.0f; n < read; n += 2) {
            dampen = (dampen >= 1.0f) ? 1.0f : dampen + inc;
            sum += (float)buf[n] * dampen;
        }
        if (((unsigned int)sum / (read / 2)) > cutoff) {
            dampen = 0.0f;
            inc = 1.0f / (read * div);
            printf("%s\n", msg);
            fflush(stdout);
        }
    }
    sio_stop(s);
    sio_close(s);
    return 0;
}
