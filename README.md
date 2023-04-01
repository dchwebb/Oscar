# Oscar
![Image](https://raw.githubusercontent.com/dchwebb/Oscar/master/Graphics/OscAB.jpg "icon")

Overview
--------

Oscar is a three channel oscilloscope, spectrum analyser and MIDI event viewer designed for use in a Eurorack modular synthesiser. Each of the three channels has a buffered through allowing use any where in the signal chain.

The oscilloscope can display up to three channels at once, either in separate lanes or overlaid. Horizontal and vertical zoom are available and a trigger with adjustable x/y can be applied to any channel.

![Image](https://raw.githubusercontent.com/dchwebb/Oscar/master/Graphics/OscA.jpg "icon")

The spectrum analyser is used to show the frequency components of any signal. The frequencies of the principal harmonics are shown both graphically and numerically. An autotune function dynamically adjusts the sample rate to lock on the fundamental frequency of the signal. This gives an extremely clear view of the frequency spectrum whilst being fast enough for real-time use.

![Image](https://raw.githubusercontent.com/dchwebb/Oscar/master/Graphics/Spectrum.jpg "icon")

A waterfall plot shows the frequency spectrum over time in a 3 dimensional view.

![Image](https://raw.githubusercontent.com/dchwebb/Oscar/master/Graphics/WaterfallA.jpg "icon")

A MIDI event viewer displays the MIDI channel, note on/off status, pitchbends, control changes and aftertouch. Clock speed is indicated with a flashing dot. 

![Image](https://raw.githubusercontent.com/dchwebb/Oscar/master/Graphics/Midi1.jpg "icon")

Technical
---------

Oscar uses an SMT32F446 microcontroller to capture incoming signals with the internal ADC and display the results on a 320x240 LCD display via SPI. Code is written in C++ 20 using the STM32CubeIDE Eclipse-based IDE. Hardware interfacing is carried out through custom code (ie no STM HAL libraries).

![Image](https://raw.githubusercontent.com/dchwebb/Oscar/master/Graphics/Components.jpg "icon")

Analog signal capture is carried out via the internal 12 bit ADCs. Capture is controlled by a timer that is adjusted to the desired horizontal display frequency in oscilloscope mode. In Spectrum Analyser mode the autotune function dynamically alters the sampling rate to attempt to capture an integer multiple of the fundamental frequency of the incoming signal. This is done by inspecting the side-lobes of the FFT and altering the sampling rate to minimise the offset.

The internal UART is used to capture MIDI data for display.

![Image](https://raw.githubusercontent.com/dchwebb/Oscar/master/Graphics/Midi2.jpg "icon")

The microcontroller is clocked at 180MHz via an external 8MHz crystal oscillator.

The display is a 320x240 TFT LCD controlled with an ILI9341 driver. Screen refreshes are sent using SPI via DMA from the microcontroller. Owing to memory limitations updates are carried out in double-buffered blocks rather than via full screen buffers.

![Image](https://raw.githubusercontent.com/dchwebb/Oscar/master/Graphics/WaterfallB.jpg "icon")

### Power Supply

The Eurorack +/-12V rails have reverse polarity protection and filtering. The 3.3V MCU and DAC digital power supplies are provided by an LD1117-3.3 linear regulator. A common digital/analog ground plane is used.

- +12V Current Draw: 157mA
- -12V Current Draw: 6mA

