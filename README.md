# bang

An attempt to sync my TR-606 to my OpenBSD machine with sndio. Very limited utility for the general public, but presented to whomever might want to hack it.

## Build

    cc bang.c -lsndio -o bang

## Usage

    bang <cutoff value 0:255> <clock divide> <message>

The program will listen to the audio input. If the amplitude is greater than the specified `cutoff`, then `message` will be displayed. I was having trouble with sndio eagerly reading audio at a high resolution no matter what values I passed to it, so `div` can be used to skip some cycles.

## Caveats

Many. Program assumes stereo input, 48000khz sample rate. sndio doesn't respect some of the settings on my soundcard/OS. Source code is like 50 lines, so just edit what doesn't please you.
