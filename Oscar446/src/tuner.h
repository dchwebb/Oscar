#pragma once

#include "initialisation.h"
#include "fft.h"




//class UI;		// forward reference to handle circular dependency



class Tuner {
public:
	enum tunerMode {ZeroCrossing, AutoCorrelation, FFT};

	Tuner();
	void Capture();							// Called from timer interrupt
	void Activate(bool startTimer);			// Tuning mode started
	void Run();								// Processes samples once collected

	encoderType encModeL = TunerMode;
	encoderType encModeR = ActiveChannel;

	tunerMode mode = ZeroCrossing;

private:
	static constexpr uint32_t zeroCrossRate = 400;		// sample rate is 90Mhz / clockDivider
	static constexpr uint32_t autoCorrRate = 1023;		// sample capture rate of auto-correlation

	bool samplesReady = false;
	uint32_t bufferPos = 0;					// Capture buffer position
	float currFreq = 0.0f;

	// Zero crossing settings
	std::array<uint32_t, 20> zeroCrossings;	// Holds the timestamps in samples of each upward zero crossing
	bool overZero = false;					// Stores current direction of waveform
	uint32_t timer  = 0;

	// Auto-correlation settings
	uint32_t samplesSize;					// Populated in constructor as we share the FFT buffer
	uint16_t* samples;						// Pointer to the FFT buffer

	static constexpr uint32_t window = 200;
	std::array<uint32_t, 1000> results;


};

extern Tuner tuner;
