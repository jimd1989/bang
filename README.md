# bang

Written as an attempt to sync my instruments (TR-606, SQ-1) to my OpenBSD machine with sndio. Not tested nor supported on any other OS. Listens to audio from microphone jack and prints custom messages to stdout when the amplitude crosses a certain threshold. Stereo aware in both input and output.

## Build

    make && make install

Might have to be root for installing.

## Usage

    bang <left cutoff [0,255]> <right cutoff [0,255]> <left message> <right message>

The program will listen to the audio input. If the average amplitude of a channel's sampled window is greater than the specified `cutoff`, then its associated `message` will be displayed.

A channel can be ignored by specifying "-" as its message.

The program is written to track square shaped pulses, which it should do okay. Try adjusting the input volume if you're having trouble following the signal.

## See also

- [boar](https://github.com/jimd1989/boar)
- [boar-extras](https://github.com/jimd1989/boar-extras)
- [boar-midi](https://github.com/jimd1989/boar-midi)
- [pop](https://github.com/jimd1989/pop)
