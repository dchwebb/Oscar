#pragma once

#include "stm32f4xx.h"
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


class fft {
public:
	float FFTBuffer[2][FFTSAMPLES];

	fft();
	void runFFT(volatile float candSin[]);
private:
	float candCos[FFTSAMPLES];
	uint16_t FFTDrawBuffer[2][(DRAWHEIGHT + 1) * FFTDRAWBUFFERSIZE];
	float SineLUT[LUTSIZE];
};


