# Kishoof Manual

## Table of Contents

- [Controls](#controls)
- [Wavetables and Playback Mode](#wavetables-and-playback-mode)
  * [Smooth Playback Mode](#smooth-playback-mode)
  * [Stepped Playback Mode](#stepped-playback-mode)
- [Warping and Modulation](#warping-and-modulation)
  * [Channel A Modulation](#channel-a-modulation)
  * [Channel B Modulation](#channel-b-modulation)
- [Startup configuration](#startup-configuration)
- [Calibration Configuration and the Serial Console](#calibration-configuration-and-the-serial-console)
  * [Calibration](#calibration)
  * [Flash Storage Management](#flash-storage-management)
  * [Safe Mode and full Flash erasing](#safe-mode-and-full-flash-erasing)
- [Firmware Upgrade](#firmware-upgrade)


## Controls

![Panel](Graphics/Panel_Annotated.png?raw=true)

**1. LCD Screen** Displays name of selected wavetable, waveforms, wavetable and warp positions and warp type

**2. Encoder** Selects wavetables; press to change display from both to either wavetable

**3. Transpose**: 3 position control allowing octave up, down or normal for both channels

**4. Playback Mode** Up position plays a smoothly interpolated wavetable in channel A and a preset wavetable in channel B; In the down position both channels can play any wavetable frame, without interpolation

**5. Channel A Wavetable** Wavetable frames are selected by a combination of the knob and CV via attenuator

**6. Channel B Wavetable** Wavetable frames are selected by a combination of the knob and CV input

**7. Channel B Modulation** Modulate channel B output: Up position mixes in channel A; down position multiplies with Channel A (ring modulation)

**8. Channel B Octave** When button is pressed (LED on) channel B is one octave down from channel A

**9. Warp Type** Selects channel A warp type (Squeeze, Bend, Mirror, Through-Zero FM). Press knob to reverse direction of channel B wavetable

**10. Warp Amount** Select the amount of modulation via knob and attenuatable CV

**11. USB C device** Used to access the internal storage as a USB drive; also provides a serial console for advanced utilities and configuration

**12. VCA input** Attach an envelope CV to control output volume of module

**13. Pitch** Attach a 1 volt per octave CV to control pitch over 6 octaves

<br />
<br />

## Wavetables and Playback Mode

Kishoof plays back wavetables from an internal 64MB flash storage system. By connecting the module to a host via USB C the internal storage can be accessed and wavetables stored.

Supported Wavetable formats:
- wav file format
- Either 32 bit floating point or 16 bit fixed point
- Mono
- 2048 samples per frame
- Up to 256 frames

Wavetables are selected with the rotary encoder. If wavetables are stored in sub-folders these will be shown in yellow text and can be accessed by pressing the encoder button. If a wavetable is corrupt or in an unsupported format an error is shown on attempting to load:

- **Fragged**: Wavetable is fragmented in the file system
- **Corrupt**: The wav header cannot be read
- **Format**: Not 32 bit floating point or 16 bit integer
- **Channels**: Not mono
- **Empty**: No file data


### Smooth Playback Mode

With the playback mode switch in the up position, Channel A will playback the selected wavetable smoothly interpolating between the frames as the wavetable position is moved.

In this mode channel B plays a special internal wavetable which is user configurable via the console. This can contain up to 10 different wavetable frames chosen from: Sine at 1, 2, 3, 4, 5 or 6 times base frequency, Square, Saw, Triangle waves. This is generated using additive synthesis and is configurable in the console - use the command **help add** for instructions.


### Stepped Playback Mode

With the playback switch in the down position, wavetable frames are selected individually and not interpolated. In this mode both channel A and B can play any frame from the same wavetable.

<br />
<br />

## Warping and Modulation

### Channel A Modulation

The Warp Type knob selects the type of modulation applied to channel A:

- **Squeeze** - Warp amount to the left stretches the wave out from the center; to the right squeezes it into the middle

- **Bend** - Progressively squashes the wave from the right to the left

- **Mirror** - The first half of the wave cycle plays the wave forwards, the second half backwards. The Warp amount stretches the wave progressively from the outside to the inside

- **TZFM** - Applies through zero frequency modulation from channel B to channel A: as channel B's wave increases channel A's wavetable is played faster and vice versa. The overall amount of frequency modulation is proportional the base pitch so this will not produce enharmonic tones (as with standard FM).

### Channel B Modulation

The channel B modulation switch affects channel B's output: In the up position channel A's signal is added/mixed  and in the down position the two signals are multiplied (ie ring modulated).

Channel B can also be reversed by clicking the Warp Type button - orange arrows are shown at the bottom of the display to indicate if this is on.

Channel B can also be switched an octave down relative to channel A. This will affect channel A in TZFM mode, unlike the add/multiple modulations.

<br />
<br />

## Startup configuration

Kishoof has a mechanism to store its current settings in the microcontroller's internal non-volatile memory. Settings that are stored are:

- Calibration data
- Currently selected wavetable
- Channel B octave button state
- Channel B reverse wavetable playback state
- UI wavetable display settings

Changes are batched and are only written after 60 seconds since the last change has elapsed. Changes are stored sequentially over the internal non-volatile memory to avoid excessive wear.

So that configuration changes do not interfere with audio output there are a maximum number of changes that can be written in a single session, after which a restart is needed to free up additional memory. Total storage for config data is 24 kBytes and each configuration block is around 48 bytes so up to 512 configuration blocks can be saved per session.

If the configuration becomes corrupt or does not write correctly even after restarting the command **clearconfig** from the console (see below) will blank the internal storage.

<br />
<br />

## Calibration Configuration and the Serial Console

When connecting a USB C cable to the module a virtual COM port provides a serial console for calibration, configuration and trouble-shooting.

Use an appropriate serial console application (Termite, Putty etc) to connect. Type 'help' for a list of commands and 'info' for module information including firmware build date, calibration settings etc:

![Console](Graphics/console.png?raw=true)

### Calibration

Calibration is used to configure the appropriate scaling for the 1 volt per octave signal. It also sets the normalisation level of the VCA control so the output is at full level with no envelope connected.

To calibrate it is necessary to generate a 0V and 1V signal which represent the lowest two C octaves the module can output. The calibration process will show its readings of the applied signals so try different octaves if the readings look out.

Type 'calib' to begin, check the measured values against the expected values and type 'y' to continue or 'x' to abort:

![Calibration](Graphics/calib.png?raw=true)

### Flash Storage Management

An internal 512Mbit NOR flash chip is used to store up to 64 MB of wavetable data. The storage must be formatted in FAT16 format and it is recommended to use the internal 'format' command to initialise the storage correctly. This also creates dummy Windows indexer files which allow the module to track and discard spurious changes that create excessive wear to the storage.

There are various commands that provide low-level trouble-shooting and diagnostic information on the internal storage. The following commands are available:

**format** reformats the internal storage, erasing all wavetables and folders

**wavetables** displays a list of all recognised wavetables and folders:

![Wavetables](Graphics/wavetables.png?raw=true)

**dirdetails** displays a detailed listing of files on the internal storage:

![Directory Details](Graphics/dirdetails.png?raw=true)



### Safe Mode and full Flash erasing

If the storage becomes badly corrupted the module may have trouble starting. In this situation access safe mode by power cycling the module with the encoder button depressed.

This will not mount the USB drive, will not mount the flash storage, will not load the configuration settings and allows complete erasing of the flash storage.

In safe mode to completely erase the internal flash storage type **eraseflash**.

<br />
<br />

## Firmware Upgrade

A mechanism is provided to upgrade the firmware over USB. Note that the latest firmware for the module can be found here: [Kishoof.elf](Kishoof/Debug). Download the file (use raw format to get the file in binary format).

In the serial console type **dfu**. This will reboot the module into DFU (Device Firmware Upgrade) mode.

Run the STM Cube Programmer software available here: [STM Cube Programmer](https://www.st.com/en/development-tools/stm32cubeprog.html).

![Programmer](Graphics/STMCubeProg1.png?raw=true)

1. Select USB connection
2. Click Refresh and select the appropriate USB port (USB1 here)
3. Click Connect

<br />
<br />

![Programmer2](Graphics/STMCubeProg2.png?raw=true)

- Click 'Open file' and navigate to the Kishoof.elf firmware file downloaded from Github.

<br />
<br />

![Programmer3](Graphics/STMCubeProg3.png?raw=true)

- Click the 'Download' button to transfer the firmware onto Kishoof.
- Restart the module. Note you can use the console command **info** to view the firmware build date to confirm an upgrade has succeeded.