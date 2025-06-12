# Oscar Manual

## Table of Contents

## Introduction

Oscar is a multi-function Eurorack module, combining oscilloscope, tuner, spectrum analyser, waterfall display and MIDI analyser.

It combines three channels each of which has an input selector button, input and buffered mult output. The MIDI analyser accepts a single serial MIDI input on 1/8" jack with unbuffered multiple. 

A USB socket allows analysis of USB MIDI data, firmware updates and a serial console with access to system configuration.

## Basic Navigation

Pressing the rotary encoders cycles through the different modes:

Oscilloscope -> Tuner -> Spectrum analyser -> Waterfall plot -> MIDI Analyser

The rotary controllers also change the parameters shown on the bottom corners of the screen. Use the Menu button to choose which parameters are controllable on each encoder (these options vary according to mode).

The Reset button reboots the module and the A, B and C buttons activate channels.

## Oscilloscope

The oscilloscope can display up to 3 channels, either in separate lanes or overlaid.

At the top right a frequency estimate is shown of the currently captured signal and at the bottom center the width of the display is shown in milliseconds.

A triggering system will align the display to the trigger point, shown by a yellow cross. If the signal has not crossed the trigger point after a second the screen will update and wait for the next trigger.

The following parameters can be assigned to the rotary encoders:

**Horiz scale** Zooms the display horizontally from 0.7ms at the fastest time to 700ms at the slowest.

**Vert scale** Zooms the display vertically from 1 volt to 12 volts peak to peak.

**Multi-Lane** Selects whether multiple channels are overlaid or shown in separate lanes.

**Trigger Y** Sets the vertical trigger position.

**Trigger X** Sets the horizontal trigger position.

**Trigger Ch** Selects which channel the trigger is active on (or no trigger to disable). Note that only a visible channel can be triggered on.

**Calib Scale** Allows calibration of the vertical scale - feed in a signal of known amplitude and set so that the volt scale matches.

**Calib Offset** Use to calibrate the zero volt position.

## Tuner

The tuner is used to tune audio rate signals. When a valid signal is received the tuner displays the musical note and the number of cents above or below concert pitch. Below is shown the Hertz measurement and an overlay of the captured signal.

A flashing dot at the top right of the screen changes each time a signal is captured and measured. The measurement time is shown in milliseconds at the bottom.

The rotary encoders provide the following options:

**Vert scale** Zooms the wave overlay display vertically.

**Tuner Mode** Two tuner modes are available. FFT is the most accurate but can be slightly slower. Zero Crossing mode checks for zero crossings in the signal which can be confused by complex waveforms, but is slightly faster in some cases.

**Trace Overlay** Chooses whether the waveform is overlaid at the bottom of the screen.

## Spectrum Analyser

The Spectrum Analyser mode is used to display the harmonic content of a signal. The fundamental frequency is shown as a white bar whose height indicates the strength of the harmonic.

Additional harmonics are displayed as coloured bars, with the frequencies of the first five harmonics shown on the right of the display.

The captured waveform can be optionally overlaid on the screen and the frequency analysis range is displayed at the bottom.

Encoder parameters:

**Vert scale** Zooms the wave overlay display vertically.

**Auto Tune** The accuracy of an FFT analysis is affected by the sampling rate. The Auto tune mechanism used here attempts to adjust the sampling rate to an exact multiple of the signal for an accurate analysis. This can be disabled, usually to cope with very complex signals, but the accuracy of the frequency measurements will be adversely affected.

**Horiz scale** If auto tune is disabled this alters the sampling rate to adjust the frequency of the spectrum being captured.

**Trace overlay** Chooses whether the waveform is overlaid.




