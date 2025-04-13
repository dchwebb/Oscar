#pragma once

#include "initialisation.h"
#include "fft.h"


class Tuner {
public:
	enum tunerMode {FFT, ZeroCrossing};

	void Capture();									// Called from timer interrupt
	void Activate(bool startTimer);					// Tuning mode started
	void Run();										// Processes samples once collected
	void ClearOverlay();
	uint32_t SerialiseConfig(uint8_t** buff);
	uint32_t StoreConfig(uint8_t* buff);

	struct Config {
		encoderType encModeL = VoltScale;
		encoderType encModeR = ActiveChannel;
		tunerMode mode = FFT;
		bool traceOverlay = true;					// Display trace overlaid on tuner display
	} config;

private:
	std::pair<float, float> FFTSingleBin(uint32_t bin);		// Calculates FFT of a single frequency bin
	void DrawOverlay();
	uint32_t OverlayVPos(float sample);

	bool samplesReady = false;
	uint32_t bufferPos = 0;							// Capture buffer position
	float currFreq = 0.0f;
	uint32_t lastValid = 0;							// Store time we last saw a good signal to show 'No signal' as appropriate
	bool convBlink = false;							// Show a flashing cursor each time a conversion has finished

	// Phase Adjusted FFT settings
	int8_t sampleRateAdj = 0;						// Used to make small adjustments to fft sample rate to avoid phase errors
	uint32_t magThreshold = 30000;					// Threshold at which a bin is significant enough to count as fundamental

	// Zero crossing settings
	static constexpr uint32_t zeroCrossRate = 400;	// sample rate is 90Mhz / clockDivider
	std::array<uint32_t, 20> zeroCrossings;			// Holds the timestamps in samples of each upward zero crossing
	bool overZero = false;							// Stores current direction of waveform
	uint32_t timer  = 0;
	static constexpr float overlayHeight = 108.0f;	// Height of trace overlay
};

extern Tuner tuner;

