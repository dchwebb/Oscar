#pragma once

#include "initialisation.h"
#include "lcd.h"

extern volatile uint16_t adcA, adcB;

class Osc {
public:
	void setDrawBuffer(uint16_t* buff1, uint16_t* buff2);

	uint16_t TriggerX = 10;
	uint16_t TriggerY = 7000;
	volatile uint16_t* TriggerTest = &adcA;		// store the currently active trigger channel as a reference for faster interrupt performance
	encoderType EncModeL = HorizScaleCoarse;
	encoderType EncModeR = Trigger_Y;
	encoderType CircEncModeL = FFTChannel;
	encoderType CircEncModeR = VoltScale;

	float Freq;
	uint16_t SampleTimer = 10;

	uint8_t DrawBufferNumber = 0;
	uint8_t CircZeroCrossings = 2, CircZeroCrossCnt = 0;
	int8_t OscDisplay = 0b111;

	volatile uint16_t capturedSamples[2] {0, 0};
	volatile int16_t drawOffset[2] {0, 0};
	volatile int8_t voltScale = 8;
	volatile bool noTriggerDraw = false;		// set to true if no trigger signal but a draw buffer is available
};

