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
	void Capture();							// Called from timer interrupt
	void Activate(bool startTimer);			// Tuning mode started
	void Run();								// Processes samples once collected

	encoderType encModeL = TunerMode;
	encoderType encModeR = ActiveChannel;

	tunerMode mode = ZeroCrossing;

	std::array<uint32_t, 20> zeroCrossings;
	bool overZero = false;
	bool samplesReady = false;
	uint32_t tunerPos = 0;
	uint32_t timer  = 0;
	float currFreq = 0.0f;

	std::array<uint16_t, 2000> samples;		// Could provisionally use fftBuffer which is 2x1024 samples
};

extern Tuner tuner;
