#pragma once

#include "initialisation.h"
#include "lcd.h"

extern volatile uint16_t adcA, adcB;

class Osc {
public:
	void setDrawBuffer(uint16_t* buff1, uint16_t* buff2);

	uint16_t TriggerX = 10;
	uint16_t TriggerY = 9000;
	//oscChannel TriggerChannel = channelA;
	volatile uint16_t* TriggerTest = &adcA;		// store the currently active trigger channel as a reference for faster interrupt performance
	encoderType EncModeL = HorizScaleCoarse;
	encoderType EncModeR = VoltScale;
	float Freq;
	uint16_t SampleTimer = 10;
	//uint16_t colBuffer[DRAWHEIGHT];

	uint16_t* DrawBuffer[2];
	uint8_t DrawBufferNumber = 0;
	int8_t OscDisplay = 0b111;
};

