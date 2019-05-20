#pragma once

#include "initialisation.h"
#include <cmath>
#include <sstream>
#include <array>
#include "lcd.h"
#include "ui.h"

#define LUTSIZE 1024
#define FFTSAMPLES 1024
#define FFTDRAWBUFFERWIDTH 80
#define FFTDRAWAFTERCALC true
#define FFTHARMONICCOLOURS 5

#define WATERFALLDRAWHEIGHT 80
#define WATERFALLSAMPLES 512
#define WATERFALLSIZE 256
#define WATERFALLBUFFERS 26

enum fftChannel {channelA, channelB};

class UI;		// forward reference to handle circular dependency
extern UI ui;
extern LCD lcd;

extern volatile uint32_t debugCount, coverageTotal, coverageTimer;
extern volatile uint32_t debugNoBuff;
extern volatile float freqFund;
extern volatile uint16_t FFTErrors;
extern volatile uint8_t captureBufferNumber, drawBufferNumber;

class FFT {
public:
	float FFTBuffer[2][FFTSAMPLES];
	uint16_t samples = FFTSAMPLES;
	bool autoTune = true;
	std::array<uint16_t, FFTHARMONICCOLOURS> harmonic;
	bool dataAvailable[2] {false, false};
	fftChannel channel = channelA;

	FFT();
	void runFFT(volatile float candSin[]);
	void waterfall(volatile float candSin[]);
	float harmonicFreq(uint16_t harmonicNumber);
	void sampleCapture(bool clearBuffer);
private:
	float candCos[FFTSAMPLES];
	float SineLUT[LUTSIZE];
	uint16_t FFTDrawBuffer[2][(DRAWHEIGHT + 1) * FFTDRAWBUFFERWIDTH];
	std::string CurrentHertz;
	uint32_t maxHyp = 0;
	uint16_t newARR = 0;
	const uint16_t harmColours[FFTHARMONICCOLOURS] {LCD_WHITE, LCD_YELLOW, LCD_GREEN, LCD_ORANGE, LCD_CYAN} ;

	uint8_t drawWaterfall[WATERFALLBUFFERS][WATERFALLSIZE];
	uint8_t waterfallBuffer = 0;

	void calcFFT(volatile float candSin[]);
	void displayFFT(volatile float candSin[]);
	void displayWaterfall(volatile float candSin[]);
//	float hypotenuse(volatile float sinArray[], uint16_t& pos);
	void FFTInfo(void);

};


