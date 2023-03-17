#pragma once

#include "initialisation.h"
#include "lcd.h"
#include "ui.h"


class UI;		// forward reference to handle circular dependency
extern UI ui;
extern LCD lcd;


class FFT {
public:
	FFT();
	void Run();

	static constexpr uint16_t fftSamples = 1024;
	static constexpr uint16_t  waterfallSamples = 512;
	static constexpr uint32_t sinLUTSize = 1024;

	// FFT and Waterfall Settings
	bool autoTune = true;								// if true will attempt to adjust sample capture time to get sample capture to align to multiple of cycle period
	bool traceOverlay = true;							// Display trace overlaid on FFT display
	oscChannel channel = channelA;
	encoderType EncModeL = FFTAutoTune;
	encoderType EncModeR = ActiveChannel;
	encoderType wfallEncModeL = HorizScale;
	encoderType wfallEncModeR = ActiveChannel;

	// FFT working variables
	float fftBuffer[2][fftSamples];						// holds raw samples captured in interrupt for FFT analysis
	uint16_t samples = fftSamples;						// specifies number of samples depending on whether in FFT or Waterfall mode
	bool dataAvailable[2] {false, false};				// stores which sample buffers contain data
	uint8_t captureBufferIndex = 0;						// Index of sample buffer being captured
	uint16_t capturePos = 0;							// Position in capture buffer

	bool capturing;

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

	void calcFFT(float* candSin);
	void displayFFT(const float* candSin);
	void displayWaterfall(const float* candSin);
	float harmonicFreq(const uint16_t harmonicNumber);
	void sampleCapture(bool clearBuffer);

public:
	constexpr auto CreateSinLUT()
	{
		std::array<float, sinLUTSize> array {};
		for (uint32_t s = 0; s < sinLUTSize; ++s){
			array[s] = sin(s * 2.0f * M_PI / sinLUTSize);
		}
		return array;
	}

};



