# bang

An attempt to sync my TR-606 to my OpenBSD machine with sndio. Very limited utility for the general public, but presented to whomever might want to hack it.

## Build

    cc bang.c -lsndio -o bang

## Usage

    bang <cutoff value 0:255> <clock divide 0.0001:inf> <message>

The program will listen to the audio input. If the average amplitude of a sampled window is greater than the specified `cutoff`, then `message` will be displayed.

The `div` argument can avoid triggering the message multiple times on a single pulse. When the message is printed, an envelope over the buffer will be engaged. All values read will be multiplied by a value ∈ [0,1] depending on the envelope position. The value of `div` is the number of buffer-read cycles that occur before the envelope increments back to 1.0. It doesn't need to be an integer.

It's finnicky, but playing with the `cutoff` and `div` values should be able to track a pulse eventually.

## Caveats

Many. Program assumes stereo input, but only reads one channel. Locked to 4000khz sample rate, 8 bit depth. sndio doesn't respect some of the settings on my soundcard/OS. The envelope is linear; I don't know if that is ideal. Source code is like 50 lines, so just edit what doesn't please you.
