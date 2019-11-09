#pragma once

#include "initialisation.h"
#include "lcd.h"
#include "ui.h"
class UI;		// forward reference to handle circular dependency
extern UI ui;
extern LCD lcd;

extern volatile uint16_t adcA;
extern volatile float vCalibScale;
extern volatile int16_t vCalibOffset;
extern volatile uint16_t CalibZeroPos;

// Class to store settings and working variables for oscilloscope and circular mode

class Osc {
public:
	void setDrawBuffer(uint16_t* buff1, uint16_t* buff2);
	void OscRun();
	void CircRun();
	uint16_t CalcVertOffset(volatile const uint16_t& vPos);
	float FreqFromPos(const uint16_t pos);

	// Oscilloscope settings
	int16_t TriggerX = 10;
	uint16_t TriggerY = 7000;
	volatile uint16_t* TriggerTest = &adcA;			// store the currently active trigger channel as a reference for faster interrupt performance
	encoderType EncModeL = HorizScale;
	encoderType EncModeR = ChannelSelect;
	uint16_t sampleTimer = 10;						// Preserves oscilloscope sample timer when switching to other modes
	int8_t oscDisplay = 0b111;
	bool multiLane = true;
	int8_t voltScale = 8;

	// Circular Mode settings
	encoderType circEncModeL = ZeroCross;
	encoderType circEncModeR = ActiveChannel;
	uint8_t CircZeroCrossings = 2;					// number of zero crossings captured for each 'frame'

	// Oscilloscope working variables
	volatile uint16_t OscBufferA[2][DRAWWIDTH], OscBufferB[2][DRAWWIDTH], OscBufferC[2][DRAWWIDTH];
	uint8_t DrawBufferNumber = 0;
	uint16_t noTriggerDraw = 0;						// set to true if no trigger signal but a draw buffer is available
	uint8_t laneCount = 1;							// holds current number of lanes to display based on number of visible channels and multi lane setting
	float Freq;										// Displays frequency of current capture based on zero crossings
	bool freqBelowZero, capturing;
	uint16_t freqCrossZero;
	volatile uint16_t capturedSamples[2] {0, 0}, bufferSamples;
	volatile int16_t drawOffset[2] {0, 0};
	uint16_t prevPixelA = 0, prevPixelB = 0, prevPixelC = 0, drawPos = 0;
	uint16_t calculatedOffsetYB = 0, calculatedOffsetYC = 0;	// Pre-Calculated offsets when in multi-lane mode

	// Circular mode working variables
	volatile uint16_t zeroCrossings[2] {0, 0};		// Used in circular mode
	uint8_t CircZeroCrossCnt = 0;					// used by interrupt to count zero crossings
	bool circDrawing[2] {false, false};
	volatile uint16_t circDrawPos[2] {0, 0};
	volatile bool circDataAvailable[2] {false, false};
	volatile float captureFreq[2] {0, 0};

};


