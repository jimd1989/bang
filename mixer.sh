#!/bin/sh

mixerctl record.adc-0:1_source=mic2
mixerctl record.adc-0:1=80,80
mixerctl outputs.mic2_dir=input
mixerctl inputs.mix3_source=dac-2:3
mixerctl outputs.hp_source=mix3
