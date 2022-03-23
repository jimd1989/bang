# bang

Written as an attempt to sync my instruments (TR-606, SQ-1) to my OpenBSD machine with sndio. Not tested nor supported on any other OS. Listens to audio from microphone jack and prints custom messages to stdout when the amplitude crosses a certain threshold. Stereo aware in both input and output.

## Build

    make && make install

## Usage

    bang <cutoff [0,255]> <mute length> <left message> <right message>

The program will listen to the audio input. If the average amplitude of a channel's sampled window is greater than the specified `cutoff`, then `message` will be displayed.

A channel can be ignored by specifying "-" as its message.

The program will ignore `mute length` sampling periods after a message has been sent. This is to avoid retriggering the message with a still decaying pulse. You almost always want to use a value of at least 2 here, it seems.

It's finnicky, but playing with the `cutoff` and `mute length` values should be able to track reasonable pulse rates.

## Caveats

I probably wouldn't rely on this for anything more than chords at the moment. Faster sequences are risky, but `mute length` helps. I need help with:

- The amplitude running average. I think there's some weird drift happening here.
- Emphasizing higher amplitudes to only track spikes in the signal.
- Determining if `mute length` should hard drop its following sample cycles (current), or if it should use some sort of envelope (previous behavior). I've had mixed results with each.
- Determining if the overall sample rate of the program should be higher or lower. I have also had mixed results here.

If you are handy with DSP, please make an issue or PR addressing these points. Thank you.

## See also

- [boar](https://github.com/jimd1989/boar)
- [boar-extras](https://github.com/jimd1989/boar-extras)
- [boar-midi](https://github.com/jimd1989/boar-midi)
- [pop](https://github.com/jimd1989/pop)
