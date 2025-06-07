#pragma once

#include "initialisation.h"
#include "lcd.h"
#include "configManager.h"

class Osc {
public:
	void OscRun();
	uint16_t CalcVertOffset(const uint16_t& vPos);
	void setTriggerChannel();
	float FreqFromPos(const uint16_t pos);
	void Capture();
	uint32_t SerialiseConfig(uint8_t** buff);
	uint32_t StoreConfig(uint8_t* buff);
	void CalcZeroSize();							// returns ADC size that corresponds to 0v

	enum Channel {A = 0, B = 1, C = 2};

	// Oscilloscope settings
	uint16_t calibZeroPos = 9985;
	uint16_t* triggerTest = &adcA;					// store the currently active trigger channel as a reference for faster interrupt performance

	// Oscilloscope working variables
	uint16_t adcA, adcB, adcC, oldAdc;
	uint16_t OscBufferA[2][lcd.drawWidth], OscBufferB[2][lcd.drawWidth], OscBufferC[2][lcd.drawWidth];
	uint8_t oscBufferNumber = 0;					// Sample buffer being used to draw from
	uint16_t noTriggerDraw = 0;						// set to true if no trigger signal but a draw buffer is available
	uint8_t laneCount = 1;							// holds current number of lanes to display based on number of visible channels and multi lane setting

	bool capturing;
	uint16_t capturedSamples[2] {0, 0};
	uint16_t bufferSamples;							// Stores number of samples captured since switching buffers to ensure triggered mode works correctly
	uint8_t captureBufferNumber = 0;				// Index of capture buffer
	uint16_t capturePos = 0;						// Position in capture buffer

	bool drawing = false;
	bool uiRefresh = false;							// Set in UI class if screen has been redrawn
	int16_t drawOffset[2] {0, 0};
	uint16_t drawPos = 0;
	//uint16_t prevPixelA = 0, prevPixelB = 0, prevPixelC = 0;

	struct SamplePos {
		uint16_t pos[3];
	};
	SamplePos prevPixel;

	struct Config {
		int16_t vCalibOffset = -3590;				// Dev board with 14k resistors: -3940, 1.499999
		float vCalibScale = 1.46f;
		int16_t triggerX = 10;
		uint16_t triggerY = 7000;
		oscChannel triggerChannel = channelA;		// holds preferred trigger channel for when that channel is not displayed
		encoderType encModeL = HorizScale;
		encoderType encModeR = VertScale;
		uint16_t sampleTimer = 10;					// Preserves oscilloscope sample timer when switching to other modes
		int8_t oscDisplay = 0b111;					// Bit set for each channel displayed
		bool multiLane = true;
		int8_t voltScale = 8;
	} cfg;

	ConfigSaver configSaver = {
		.settingsAddress = &cfg,
		.settingsSize = sizeof(cfg),
		.validateSettings = nullptr
	};

private:
	void SetDrawBuffer(uint16_t* buff1, uint16_t* buff2);
	void CircRun();
	SamplePos VertOffsets(uint16_t offsetX);
	void FreqCalc(const uint16_t offsetX);

	static constexpr uint32_t textOffsetTop = 11;			// 11 offset for voltage and frequency written at top of screen
	static constexpr uint32_t textOffsetLeft = 27;			// 27 offset for voltage at left of screen
	static constexpr uint32_t textOffsetRight = 250;		// offset for frequency at right of screen

	uint8_t drawBufferNumber = 0;
	int8_t oldVoltScale = 0;						// To limit redraws of voltage information

	float freq;										// Holds frequency of current capture based on zero crossings
	bool freqBelowZero;
	uint16_t freqCrossZero;
	uint16_t freqSmoothY;							// Used in basic filter to remove high frequency noise

	uint16_t calculatedOffsetYB = 0, calculatedOffsetYC = 0;	// Pre-Calculated offsets when in multi-lane mode

};


extern Osc osc;
