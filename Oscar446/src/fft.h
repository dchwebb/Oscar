#pragma once

#include "initialisation.h"
#include "lcd.h"
#include "ui.h"

#define LUTSIZE 1024
#define FFTSAMPLES 1024
#define FFTHARMONICCOLOURS 5

#define WATERFALLDRAWHEIGHT 80
#define WATERFALLSAMPLES 512
#define WATERFALLSIZE 256
#define WATERFALLBUFFERS 26
#define WATERFALLSMOOTH 4

class UI;		// forward reference to handle circular dependency
extern UI ui;
extern LCD lcd;

extern volatile uint8_t captureBufferNumber, drawBufferNumber;
extern uint16_t DrawBuffer[2][(DRAWHEIGHT + 1) * DRAWBUFFERWIDTH];

class FFT {
public:
	FFT();
	void Run();

	// FFT and Waterfall Settings
	bool autoTune = true;								// if true will attempt to adjust sample capture time to get sample capture to align to multiple of cycle period
	bool traceOverlay = true;							// Display trace overlaid on FFT display
	oscChannel channel = channelA;
	encoderType EncModeL = FFTAutoTune;
	encoderType EncModeR = ActiveChannel;
	encoderType wfallEncModeL = HorizScale;
	encoderType wfallEncModeR = ActiveChannel;

	// FFT working variables
	float FFTBuffer[2][FFTSAMPLES];						// holds raw samples captured in interrupt for FFT analysis
	uint16_t samples = FFTSAMPLES;				// specifies number of samples depending on whether in FFT or Waterfall mode
	std::array<uint16_t, FFTHARMONICCOLOURS> harmonic;	// holds harmonics found in FFT
	volatile bool dataAvailable[2] {false, false};		// stores which sample buffers contain data
	float SineLUT[LUTSIZE];
	bool capturing;

private:
	float candCos[FFTSAMPLES];
	float freqFund = 0.0f;

	uint32_t maxHyp = 0;
	uint16_t newARR = 0;
	uint16_t FFTErrors = 0;
	const uint16_t harmColours[FFTHARMONICCOLOURS] {LCD_WHITE, LCD_YELLOW, LCD_GREEN, LCD_ORANGE, LCD_CYAN} ;

	uint8_t drawWaterfall[WATERFALLBUFFERS][WATERFALLSIZE];
	uint8_t waterfallBuffer = 0;

	void calcFFT(volatile float candSin[]);
	void displayFFT(const float* candSin);
	void displayWaterfall(const float* candSin);
	float harmonicFreq(uint16_t harmonicNumber);
	void sampleCapture(bool clearBuffer);


};



