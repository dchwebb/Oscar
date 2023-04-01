#pragma once

#include "initialisation.h"
#include "lcd.h"
#include "ui.h"


class UI;		// forward reference to handle circular dependency
extern UI ui;
extern LCD lcd;


class Tuner {
public:
	enum tunerMode {ZeroCrossing, AutoCorrelation, FFT};
	void Capture();				// Called from timer interrupt
	void Activate();			// Tuning mode started
	void Run();					// Processes samples once collected

	encoderType encModeL = TunerMode;
	encoderType encModeR = ActiveChannel;

	tunerMode mode = ZeroCrossing;

	std::array<uint32_t, 50> zeroCrossings;
	bool overZero = false;
	bool samplesReady = false;
	uint32_t zCrossPos = 0;
	uint32_t timer  = 0;
	uint32_t highThreshold = 8192;
	uint32_t lowThreshold;
	float currFreq = 0.0f;
};

extern Tuner tuner;
