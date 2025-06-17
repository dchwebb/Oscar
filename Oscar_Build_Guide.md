# Oscar Build Guide

## Table of Contents

## Introduction

Oscar v3 has been optimised as far as possible for DIY building. All surface-mount components (with the exception of the USB socket) can be assembled using JLC PCB's assembly service.

The module is constructed of 3 PCBs: 

- Component and control PCB: a four-layer PCB with surface-mount components on the back and the through-hole controls on the front.

- LCD daughterboard: this is used to raise the LCD display to the level of the front panel. The LCD panel's flexable PCB is soldered to the daughterboard which is then attached to the component PCB in four corners.

- Panel: There are two variants - one where USB-C is not needed. See the [USB Socket](#usb-socket) section for details.

As the module is based around an STM32 microcontroller you will need an ST-Link programmer (or clone, available on e-bay for under $10).

These are the three PCBs, one partly assembled by JLC PCB, the LCD screen and the ST-Link Programmer:

![PCBs, LCD and programmer](Graphics/PCBs.jpg?raw=true)

## Bill of Materials (BOM)

The BOM lists the various suppliers and part numbers that are needed to build Oscar.

- [Bill of Materials](BOM.md)

![Through-hole Controls](Graphics/through_hole_controls.jpg?raw=true)

## PCB Ordering and Assembly

The gerber files and JLC PCB assembly files are available here (use the latest versions where multiple available):

- [Panel and Daughterboard Gerbers](Hardware_v3/Gerbers)
- [Component PCB gerbers and production files](Hardware_v3/jlcpcb/production_files)

If using JLC to assemble the surface-mount components select the 'Assemble bottom side' option. Note that 2 assembled boards is the minimum that can be selected.

It is recommended to use the 'Lead free HASL' process on the panel to avoid the risk of touching a lead metal finish when operating the module (though it adds $1.20 to the cost). Select 'Black' as PCB colour.

At June 2025 the JLC costings with full SMD assembly are:

- Component board, parts and assembly (5 PCBs, 2 assembled): $48.94
- Panel (5 PCBs): $8.70
- Daughter board (5 PCBs): $2.00

JLC support two types of component: 'Basic' and 'Extended'. If you are happy hand-soldering surface-mount components you can exclude some or all of the extended parts to reduce costs (each extended part has a surcharge of $3). The cost of the assembled boards can be reduced to $30.59 by excluding all extended parts except the microcontroller.

These figures are exclusive of sales tax and shipping.

## Component PCB initial assembly

Whether you are hand-soldering the surface-mount parts or using JLC assembly it is recommended to carry out some basic testing at this stage of the build.

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

The optional USB socket should be installed next (if required) as you cannot reach the pins once the jack sockets are installed.

The USB-C socket is one of the hardest components to hand-solder as the pins are very small and the through-hole legs get in the way of some of the pins.

The USB socket is used to carry out firmware updates over USB (easier than using the ST-Link programmer), for analysing USB MIDI data and to provide a serial console for some basic configuration and diagnostics. None of these functions are critical so the installation of the socket is optional. If you are not installing the socket then a variant panel is available with the USB socket hole covered.

My preferred soldering technique is to first apply a small amount of solder to the pads. I then apply flux and position the socket.

![Prepare PCB](Graphics/usb_soldering1.jpg?raw=true)

Using a small bevelled soldering iron tip (eg Hakko T18-CF15) tack down a couple of the USB socket pins to one side. check the alignment of the pins on the other side and then tack that side down too. Once the socket is secured by a couple of pins on each side I go round soldering the more awkward corner pins.

I find angling the PCB while applying the solder helps see where the tip of the soldering iron needs to go. Note that the majority of the pins on the socket are unused, but the four corner pins are the ground connections which are important (and the most difficult to reach).

![Solder Socket](Graphics/usb_soldering2.jpg?raw=true)

Once all the pins are secured then the four through-hole legs can be soldered to the PCB for mechanical strength.

To test, connect the USB socket to a computer and apply power to the Eurorack power connector. If you have correctly installed the firmware and the USB socket is working the device should show up as a MIDI and serial USB device. In Windows:

![Device Manager](Graphics/Device_Manager.png?raw=true)



## Additional Controls

The remaining through-hole components can now be soldered to the Component PCB. First place them in their holes.

*Important* The through hole controls are mounted on the opposite side of the PCB to the surface-mount components:

![Controls Placed](Graphics/controls_placed.jpg?raw=true)

Ensure that the three illuminated buttons are rotated so the notch in the PCB matches the peg on the underside of the button. The LEDs will not work if the buttons are reversed. The colour order of the buttons is Green, Blue, Orange as you look from the panel side of the module.

Note that the rotary encoders are not high enough to properly support the panel. I used a couple of washers and a nut to achieve the correct height.

Then attach the panel to ensure correct alignment and secure with a nut and an elastic band. Turn the board over and solder the controls in place.

![Controls Secured](Graphics/controls_secured.jpg?raw=true)


## LCD Daughterboard

There appear to be a couple of variants of the LCD available. Look on Ebay (or elsewhere) for '2.4 inch TFT HD Color LCD Screen 2.8-3.3V 240x320 4-wire SPI interface ILI9341'. It should look like this:

![LCD Front](Graphics/lcd1.png?raw=true)
![LCD Back](Graphics/lcd2.png?raw=true)

It is recommended to solder the 4 pin headers to the daughterboard before soldering on the LCD. Initially only solder one pin on each header so the angle can be tweaked to ensure the daughterboard fits cleanly into the Component PCB. Once the headers are aligned correctly solder the remaining pins.

The LCD can now be soldered to the daughterboard. There is a white line above and below the PCB pads. This indicates the location of the edge of the flex-pcb connector. Put a little solder on the pads and then solder the flex-pcb connector to the pads.

The daughterboard should be attached to the component PCB before sticking down the LCD. Place the daughterboard in the four holes, put the panel on and turn the assembly upside down.

Ensure the LCD is lying flat against the panel and then solder the four pin headers. Again it is recommended to only solder one pin on each header so that adjustments can be made if needed. Once the alignment looks correct solder all pins.

The LCD can now be tested by applying Eurorack power (assuming you have correctly installed the firmware). If the LCD backlight comes on but there is no display check the soldering of the flex-pcb connector.

Once everything is working correctly the LCD can be aligned as accurately as possible and the stuck down with the adhesive pads on the back.

