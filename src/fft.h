#pragma once

#include "initialisation.h"
#include <cmath>
#include "lcd.h"


#define LUTSIZE 1024
#define FFTSAMPLES 1024
#define FFTDRAWBUFFERSIZE 160
#define FFTDRAWAFTERCALC true

constexpr int FFTbits = log2(FFTSAMPLES);
constexpr float FFTWidth = (FFTSAMPLES / 2) > DRAWWIDTH ? 1 : (float)DRAWWIDTH / (FFTSAMPLES / 2);

extern Lcd lcd;
extern volatile uint32_t debugCount, coverageTotal, coverageTimer;
extern volatile uint16_t maxHarm;

class fft {
public:
	float FFTBuffer[2][FFTSAMPLES];
	bool autoTune = true;
	uint32_t maxHyp = 0;
	//uint32_t maxHarm = 0;

	fft();
	void runFFT(volatile float candSin[]);
private:
	float candCos[FFTSAMPLES];
	uint16_t FFTDrawBuffer[2][(DRAWHEIGHT + 1) * FFTDRAWBUFFERSIZE];
	float SineLUT[LUTSIZE];
	uint8_t tunePass = 0;
};


