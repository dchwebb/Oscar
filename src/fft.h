#pragma once

#include "initialisation.h"
#include <cmath>
#include <sstream>
#include <array>
#include "lcd.h"
#include "ui.h"

#define LUTSIZE 1024
#define FFTSAMPLES 1024
#define FFTDRAWBUFFERSIZE 160
#define FFTDRAWAFTERCALC true

constexpr int FFTbits = log2(FFTSAMPLES);
constexpr float FFTWidth = (FFTSAMPLES / 2) > DRAWWIDTH ? 1 : (float)DRAWWIDTH / (FFTSAMPLES / 2);

extern Lcd lcd;
extern ui UI;
extern volatile uint32_t debugCount, coverageTotal, coverageTimer;
extern volatile uint32_t diff;
extern volatile float freqFund;

class fft {
public:
	float FFTBuffer[2][FFTSAMPLES];
	bool autoTune = true;
	uint16_t fundHarmonic = 0;
	std::array<uint16_t, 4> harmonic;

	fft();
	void runFFT(volatile float candSin[]);
private:
	float candCos[FFTSAMPLES];
	uint16_t FFTDrawBuffer[2][(DRAWHEIGHT + 1) * FFTDRAWBUFFERSIZE];
	float SineLUT[LUTSIZE];
	uint8_t tunePass = 0;
	uint32_t maxHyp = 0;
	uint16_t newARR = 0;
	uint16_t drawFFTOffset = 0;

	uint16_t harmonic2 = 0;
	uint16_t harmonic3 = 0;
	uint16_t harmonic4 = 0;
};


