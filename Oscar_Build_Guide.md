# Oscar Build Guide

## Table of Contents

## Introduction

Oscar v3 has been optimised as far as possible for DIY building. All surface-mount components (with the exception of the USB socket) can be assembled using JLC PCB's assembly service.

The module is constructed of 3 PCBs: 

- Component and control PCB: a four-layer PCB with surface mount components on the back and the through-hole controls on the front.

- LCD daughterboard: this is used to raise the LCD display to the level of the front panel. The LCD panel's flexable PCB is soldered to the daughterboard which is then attached to the component PCB in four corners.

- Panel: This is a PCB with black solder mask and exposed metal to form the UI elements.

## PCB Assembly

The gerber files and JLC PCB assembly files are available here (use the latest versions where multiple available):

- [Panel and Daughterboard Gerbers](Hardware_v3/Gerbers)
- [Component PCB gerbers and production files](Hardware_v3/jlcpcb/production_files)

If using JLC to assemble the surface mount components select the 'Assemble bottom side' option. Note that 2 assembled boards is the minimum that can be selected.

JLC support two types of component: 'Basic' and 'Extended'. If you are happy hand-soldering surface mount components you can exclude some or all of the extended parts to reduce costs (each extended part has a surcharge of $3).

It is recommended to use the 'Lead free HASL' process on the panel to avoid the risk of touching a lead metal finish when operating the module (though it adds $1.20 to the cost).

At June 2025 the JLC costing are:

Component board, parts and assembly: $48.94
Panel: $8.70
Daughter board: $2.00

These figures are exclusive of sales tax and shipping.


