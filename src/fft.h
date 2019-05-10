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
constexpr int FFTbits = log2(FFTSAMPLES);
constexpr float FFTWidth = (FFTSAMPLES / 2) > DRAWWIDTH ? 1 : (float)DRAWWIDTH / (FFTSAMPLES / 2);

extern LCD lcd;
extern UI ui;
extern volatile uint32_t debugCount, coverageTotal, coverageTimer;
extern volatile uint32_t diff;
extern volatile float freqFund;
extern volatile uint16_t FFTErrors;

class FFT {
public:
	float FFTBuffer[2][FFTSAMPLES];
	bool autoTune = true;
	std::array<uint16_t, FFTHARMONICCOLOURS> harmonic;

	FFT();
	void runFFT(volatile float candSin[]);
	float harmonicFreq(uint16_t harmonicNumber);
private:
	float candCos[FFTSAMPLES];
	float SineLUT[LUTSIZE];
	uint16_t FFTDrawBuffer[2][(DRAWHEIGHT + 1) * FFTDRAWBUFFERWIDTH];
	uint32_t maxHyp = 0;
	uint16_t newARR = 0;

	const uint16_t harmColours[FFTHARMONICCOLOURS] {LCD_WHITE, LCD_YELLOW, LCD_GREEN, LCD_ORANGE, LCD_CYAN} ;

};


