# Oscar Build Guide

## Table of Contents

## Introduction

Oscar v3 has been optimised as far as possible for DIY building. All surface-mount components (with the exception of the USB socket) can be assembled using JLC PCB's assembly service.

The module is constructed of 3 PCBs: 

- Component and control PCB: this is a four-layer board but with size constrained to 100mm for cost effective building by JLC. Surface mount components are placed on the back and the through-hole controls on the front.

- LCD daughterboard: this is used to raise the LCD display to the level of the front panel. The LCD panel's flexable PCB is soldered to the daughterboard which is then attached to the component PCB in four corners.

- Panel: This is a PCB with black solder mask and exposed metal to form the UI elements.

## PCB Assembly

The gerber files and JLC PCB assembly files are available here:

- [Panel and Daughterboard Gerbers](Hardware_v3/Gerbers)
- [Component PCB gerbers and production files](Hardware_v3/jlcpcb)

