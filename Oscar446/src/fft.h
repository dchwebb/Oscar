#pragma once

#include "initialisation.h"
#include "lcd.h"
#include "ui.h"

class Tuner;

class FFT {
	friend class Tuner;

public:

	FFT();
	void Run();
	void Capture();
	void Activate();
	uint32_t SerialiseConfig(uint8_t** buff);
	uint32_t StoreConfig(uint8_t* buff);

	static constexpr uint32_t fftSamples = 1024;
	static constexpr uint32_t  waterfallSamples = 512;
	static constexpr uint32_t sinLUTSize = 1024;
	static constexpr uint32_t timerDefault = 10000;		// Default speed of sample capture (start fairly slow) sample rate is 90Mhz / clockDivider

	// FFT and Waterfall Settings
	encoderType wfallEncModeL = HorizScale;
	encoderType wfallEncModeR = ActiveChannel;

	// FFT working variables
	float fftBuffer[2][fftSamples];						// holds raw samples captured in interrupt for FFT analysis
	uint32_t samples = fftSamples;						// specifies number of samples depending on whether in FFT or Waterfall mode
	bool dataAvailable[2] {false, false};				// stores which sample buffers contain data
	uint8_t captureBufferIndex = 0;						// Index of sample buffer being captured
	uint16_t capturePos = 0;							// Position in capture buffer

	bool capturing;

	const float* sinLUTExt = nullptr;					// As LUT is created constexpr store a pointer to allow access from outside FFT class

	struct Config {
		bool autoTune = true;							// if true will attempt to adjust sample capture time to get sample capture to align to multiple of cycle period
		bool traceOverlay = true;						// Display trace overlaid on FFT display
		oscChannel channel = channelA;
		encoderType encModeL = VoltScale;
		encoderType encModeR = ActiveChannel;
	} config;

private:
	static constexpr uint8_t  fftHarmonicColours = 5;
	static constexpr uint16_t waterfallDrawHeight = 80;
	static constexpr uint16_t waterfallSize = 256;
	static constexpr uint16_t waterfallBuffers = 26;
	static constexpr uint16_t waterfallSmooth = 4;

	float cosBuffer[fftSamples];						// Stores working cosine part of FFT calculation
	uint8_t fftBufferNumber = 0;						// Index of sample buffer used for calculations
	uint16_t fftErrors = 0;

	const uint16_t harmColours[fftHarmonicColours] {LCD_WHITE, LCD_YELLOW, LCD_GREEN, LCD_ORANGE, LCD_CYAN} ;
	std::array<uint16_t, fftHarmonicColours> harmonic;	// holds index of harmonics found in FFT

	uint8_t drawWaterfall[waterfallBuffers][waterfallSize];
	uint8_t waterfallBuffer = 0;

	void CalcFFT(float* sinBuffer, uint32_t samples);
	void PopulateOverlayBuffer(const float* sinBuffer);
	void DisplayFFT(const float* candSin);
	void DisplayWaterfall(const float* sinBuffer);
	float HarmonicFreq(const float harmonicNumber);
	void readyCapture(bool clearBuffer);

public:
	constexpr auto CreateSinLUT()						// constexpr function to generate LUT in Flash
	{
		std::array<float, sinLUTSize> array {};
		for (uint32_t s = 0; s < sinLUTSize; ++s){
			array[s] = sin(s * 2.0f * M_PI / sinLUTSize);
		}
		return array;
	}

};


extern FFT fft;
