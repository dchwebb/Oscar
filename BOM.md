# Oscar v3 BOM

## Screen

[2.4 inch TFT HD Color LCD Screen 2.8-3.3V 240x320 4-wire SPI interface ILI9341](https://www.ebay.co.uk/itm/394725476377)
Various lengths of 2.54mm pin headers

## Generic Parts (optional if using JLC assembly)

|Comment      |Designator          |Footprint                                  |LCSC    |
|-------------|--------------------|-------------------------------------------|--------|
|8MHz         |Y1                  |Crystal 5032 2 Pin 5.0x3.2mm               |C115962 |
|STM32F446RETx|U3                  |LQFP-64_10x10mm_P0.5mm                     |C69336  |
|H11L1        |U6                  |H11L1 Optocoupler                          |C20082  |
|RCLAMP0502B  |D4                  |SOT-416 ESD Protection for USB             |C5370962|
|LT1117-3.3   |U1                  |SOT-223-3 TabPin2 Linear Regulator         |C6186   |
|TL074        |U4                  |SOIC-14_3.9x8.7mm_P1.27mm OpAmp            |C107644 |
|MCP6004      |U5                  |SOIC-14_3.9x8.7mm_P1.27mm OpAmp            |C7378   |
|1N4148W      |D3                  |D_SOD-123 Diode                            |C81598  |
|1N5819       |D1,D2               |D_SOD-123 Diode                            |C8598   |
|Ferrite Bead |L1                  |0805 Inductor                              |C1017   |
|22uF 16V     |C2,C8               |Electrolytic Capacitor 4 x 5.8mm           |C134793 |
|4.7uF        |C1,C11,C15,C9       |0805 Capacitor                             |C1779   |
|1uF          |C14                 |0805 Capacitor                             |C28323  |
|100nF        |C10,C12,C13,C3,C6,C7|0805 Capacitor                             |C49678  |
|30pF         |C4,C5               |0603 Capacitor                             |C1658   |
|100          |R13,R14,R15         |0805 1% Resistor                           |C17408  |
|150          |R1                  |0805 1% Resistor                           |C17471  |
|220          |R16                 |0805 1% Resistor                           |C17557  |
|470          |R17                 |0805 1% Resistor                           |C17710  |
|1.5k         |R18,R19,R20         |0805 1% Resistor                           |C4310   |
|10k          |R11,R12             |0805 1% Resistor                           |C17414  |
|15k          |R10,R8,R9           |0805 1% Resistor                           |C17475  |
|100k         |R2,R3,R4,R5,R6,R7   |0805 1% Resistor                           |C149504 |

## Digikey/Mouser Components

|Comment      |Designator          |Footprint                                   |Digikey               |Mouser              |
|-------------|--------------------|--------------------------------------------|----------------------|--------------------|
|Menu Button  |SW2                 |Tactile Switch SKHHDTA010                   |4809-SKHHDTA010-ND    |688-SKHHDT          |
|Reset Button |SW1                 |Tactile Switch SKHHDTA010                   |4809-SKHHDTA010-ND    |688-SKHHDT          |
|Right Encoder|SW4                 |Rotary Encoder PEC11R-4220F-S0024           |PEC11R-4220F-S0024-ND |652-PEC11R-4220F-S24|
|Left Encoder |SW3                 |Rotary Encoder PEC11R-4220F-S0024           |PEC11R-4220F-S0024-ND |652-PEC11R-4220F-S24|
|USB C Socket |J11                 |Amphenol 12401951E412A                      |664-12401951E412ATR-ND|523-12401951E412A   |
|Eurorack 10 pin power|J2          |Eurorack 10 pin shrouded header             |SAM10831-ND           |200-TST10501TD      |


## Thonk Components

|Comment      |Designator          |Footprint                                  |Thonk   |
|-------------|--------------------|-------------------------------------------|--------|
|ChA Btn      |BTN1                |Low Profile LED Button                     |[Low-Profile LED Button - GREEN](https://www.thonk.co.uk/shop/low-profile-led-buttons/)|
|ChB Btn      |BTN2                |Low Profile LED Button                     |[Low-Profile LED Button - BLUE](https://www.thonk.co.uk/shop/low-profile-led-buttons/)|
|ChC Btn      |BTN3                |Low Profile LED Button                     |[Low-Profile LED Button - ORANGE](https://www.thonk.co.uk/shop/low-profile-led-buttons/)|
|ChannelA In  |J4                  |THONKICONN hole                            |[Thonkiconn Mono 3.5mm Audio Jacks (WQP518MA)](https://www.thonk.co.uk/shop/thonkiconn/)|
|ChannelB In  |J5                  |THONKICONN hole                            |[Thonkiconn Mono 3.5mm Audio Jacks (WQP518MA)](https://www.thonk.co.uk/shop/thonkiconn/)|
|ChA Buff Out |J7                  |THONKICONN hole                            |[Thonkiconn Mono 3.5mm Audio Jacks (WQP518MA)](https://www.thonk.co.uk/shop/thonkiconn/)|
|ChB Buff Out |J8                  |THONKICONN hole                            |[Thonkiconn Mono 3.5mm Audio Jacks (WQP518MA)](https://www.thonk.co.uk/shop/thonkiconn/)|
|ChC Buff Out |J6                  |THONKICONN hole                            |[Thonkiconn Mono 3.5mm Audio Jacks (WQP518MA)](https://www.thonk.co.uk/shop/thonkiconn/)|
|ChannelC In  |J3                  |THONKICONN hole                            |[Thonkiconn Mono 3.5mm Audio Jacks (WQP518MA)](https://www.thonk.co.uk/shop/thonkiconn/)|
|MIDI IN      |J9                  |Thonkiconn Stereo-PJ366ST                  |[Green Thonkiconn Stereo 3.5mm Audio Jacks (PJ366ST)](https://www.thonk.co.uk/shop/thonkiconn/)|
|MIDI THROUGH |J10                 |Thonkiconn Stereo-PJ366ST                  |[Green Thonkiconn Stereo 3.5mm Audio Jacks (PJ366ST)](https://www.thonk.co.uk/shop/thonkiconn/)|

