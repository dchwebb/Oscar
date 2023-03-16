#pragma once

#include "initialisation.h"
#include "lcd.h"
#include "ui.h"
class UI;		// forward reference to handle circular dependency
extern UI ui;
extern LCD lcd;

extern volatile uint8_t captureBufferNumber;

// Class to store settings and working variables for oscilloscope mode

class Osc {
public:
	void setDrawBuffer(uint16_t* buff1, uint16_t* buff2);
	void OscRun();
	void CircRun();
	uint16_t CalcVertOffset(const uint16_t& vPos);
	float FreqFromPos(const uint16_t pos);
	void setTriggerChannel();

	// Oscilloscope settings
	int16_t triggerX = 10;
	uint16_t triggerY = 7000;
	uint16_t* triggerTest = &adcA;			// store the currently active trigger channel as a reference for faster interrupt performance
	oscChannel triggerChannel = channelA;			// holds preferred trigger channel for when that channel is not displayed
	encoderType encModeL = HorizScale;
	encoderType encModeR = ChannelSelect;
	uint16_t sampleTimer = 10;						// Preserves oscilloscope sample timer when switching to other modes
	int8_t oscDisplay = 0b111;
	bool multiLane = true;
	int8_t voltScale = 8;

	// Oscilloscope working variables
	uint16_t OscBufferA[2][lcd.drawWidth], OscBufferB[2][lcd.drawWidth], OscBufferC[2][lcd.drawWidth];
	uint8_t oscBufferNumber = 0;
	uint8_t DrawBufferNumber = 0;
	uint16_t noTriggerDraw = 0;						// set to true if no trigger signal but a draw buffer is available
	uint8_t laneCount = 1;							// holds current number of lanes to display based on number of visible channels and multi lane setting
	float freq;										// Displays frequency of current capture based on zero crossings
	bool freqBelowZero, capturing;
	uint16_t freqCrossZero;
	uint16_t capturedSamples[2] {0, 0}, bufferSamples;
	int16_t drawOffset[2] {0, 0};
	uint16_t prevPixelA = 0, prevPixelB = 0, prevPixelC = 0, drawPos = 0;
	uint16_t calculatedOffsetYB = 0, calculatedOffsetYC = 0;	// Pre-Calculated offsets when in multi-lane mode


};


