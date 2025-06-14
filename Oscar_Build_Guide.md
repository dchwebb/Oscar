# Oscar Build Guide

## Table of Contents

## Introduction

Oscar v3 has been optimised as far as possible for DIY building. All surface-mount components (with the exception of the USB socket) can be assembled using JLC PCB's assembly service.

The module is constructed of 3 PCBs: 

- Component and control PCB: a four-layer PCB with surface mount components on the back and the through-hole controls on the front.

- LCD daughterboard: this is used to raise the LCD display to the level of the front panel. The LCD panel's flexable PCB is soldered to the daughterboard which is then attached to the component PCB in four corners.

- Panel: There are two variants - one where USB-C is not needed. See the [USB Socket](#usb-socket) section for details.

As the module is based around an STM32 microcontroller you will need an ST-Link programmer (or clone, available on e-bay for under $10).

These are the three PCBs, one partly assembled by JLC PCB and the ST-Link Programmer:

![PCBs, LCD and programmer](Graphics/PCBs.jpg?raw=true)

## Bill of Materials (BOM)

The BOM lists the various suppliers and part numbers that are needed to build Oscar.

- [Bill of Materials](BOM.md)


## PCB Ordering and Assembly

The gerber files and JLC PCB assembly files are available here (use the latest versions where multiple available):

- [Panel and Daughterboard Gerbers](Hardware_v3/Gerbers)
- [Component PCB gerbers and production files](Hardware_v3/jlcpcb/production_files)

If using JLC to assemble the surface mount components select the 'Assemble bottom side' option. Note that 2 assembled boards is the minimum that can be selected.

JLC support two types of component: 'Basic' and 'Extended'. If you are happy hand-soldering surface mount components you can exclude some or all of the extended parts to reduce costs (each extended part has a surcharge of $3).

It is recommended to use the 'Lead free HASL' process on the panel to avoid the risk of touching a lead metal finish when operating the module (though it adds $1.20 to the cost). Select 'Black' as PCB colour.

At June 2025 the JLC costings with full SMD assembly are:

- Component board, parts and assembly (5 PCBs, 2 assembled): $48.94
- Panel (5 PCBs): $8.70
- Daughter board (5 PCBs): $2.00

If you are happy hand-soldering some SMD parts you could reduce the cost of the assembled boards to $30.59 by excluding all extended parts except the microcontroller.

These figures are exclusive of sales tax and shipping.

## Component PCB initial assembly

Whether you are hand-soldering the surface mount parts or using JLC assembly it is recommended to carry out some basic testing at this stage of the build.

### Power testing

Firstly check for shorts on the power rails using a multimeter in resistance mode using the test points at the top right of the PCB. 

The resistance from the +12V and -12V to ground should be in the kilo-ohm range (and will change as capacitors charge). The resistance from the 3.3V rail to ground should be around 300ohm.
 
Solder on the 10 pin Eurorack connector and connect to a Eurorack power supply. Test the 3 voltage rails using a multimeter in DC volt mode. Note that the 12 volt rails will measure slightly low owing the drop in the reverse polarity protection diodes.

### Microcontroller

It is also recommended to check that the STM32 microcontroller is working. Solder on the programming header (marked '3.3V, GND, IO, CLK' at the top left) using a 4 way 2.54mm pin header.

Connect an ST-Link Programmer to the programming header. There are 10 connections on the ST-Link v2 programmer but you only need to connect GND to GND, SWDIO to IO, SWCLK to CLK. Use the 3.3v connection from the programmer if you are not powering from a Eurorack power supply.

Run the STM Cube Programmer software (available here: [STM Cube Programmer](https://www.st.com/en/development-tools/stm32cubeprog.html)).

1. Ensure that ST-LINK is selected at the top right and click 'Connect'

![Programmer](Graphics/STMCubeProg_stlink1.png?raw=true)

2. Check that the target information in the bottom right is correct - this means that the programmer has found the microcontroller and that it is functional.

![Programmer](Graphics/STMCubeProg_stlink2.png?raw=true)

3. Click 'Open File' (top left) and navigate to the Oscar_v3.elf firmware file downloaded from Github and then click the 'Download' button (to the top right).

![Programmer](Graphics/STMCubeProg_stlink3.png?raw=true)

If all goes well the microcontroller will be working and loaded with the current firmware.

## USB socket

The optional USB socket should be installed next (if required) as you cannot reach the pins once the jact sockets are installed

The USB-C socket is one of the hardest components to hand-solder as the pins are very small and the through-hole legs get in the way of some of the pins.

The USB socket is used to carry out firmware updates over USB (rather than using the ST-Link programmer), for analysing USB MIDI data and to provide a serial console for some basic configuration and diagnostics.

None of these functions are critical so the installation of the socket is optional. If you are not installing the socket then a variant panel is available with the USB socket hole covered.

## Additional Controls

The remaining through-hole components can now be soldered to the Component PCB. it is recommended to place them in their holes, attach the panel to ensure correct alignment and then solder the controls in place.

Note that the rotary encoders are not high enough to properly support the panel. I used a couple of washers and a nut to achieve the correct height.

## LCD Daughterboard

There appear to be a couple of variants of the LCD available. I bought mine on Ebay with the search term '2.4 inch TFT HD Color LCD Screen 2.8-3.3V 240x320 4-wire SPI interface ILI9341'.

![LCD Front](Graphics/lcd1.png?raw=true)
![LCD Back](Graphics/lcd2.png?raw=true)

It is recommended to solder the 4 pin headers to the daughterboard before soldering on the LCD. Initially only solder one pin on each header so the angle can be tweaked to ensure the daughterboard fits cleanly into the Component PCB. Once the headers are aligned correctly solder the remaining pins.

The LCD can now be soldered to the daughterboard. There is a white line above and below the PCB pads. This indicates the location of the edge of the flex-pcb connector. Put a little solder on the pads and then solder the flex-pcb connector to the pads.

The daughterboard should be attached to the component PCB before sticking down the LCD. Place the daughterboard in the four holes, put the panel on and turn the assembly upside down.

Ensure the LCD is lying flat against the panel and then solder the four pin headers. Again it is recommended to only solder one pin on each header so that adjustments can be made if needed. Once the alignment looks correct solder all pins.

The LCD can now be tested by applying Eurorack power (assuming you have correctly installed the firmware). If the LCD backlight comes on but there is no display check the soldering of the flex-pcb connector.

Once everything is working correctly the LCD can be aligned as accurately as possible and the stuck down with the adhesive pads on the back.

