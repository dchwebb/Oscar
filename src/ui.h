#pragma once

#include <string>
#include <sstream>
#include <cmath>
#include "lcd.h"
#include "fft.h"
#include "initialisation.h"

extern void ResetMode();

class FFT;		// forward reference to handle circular dependency
extern FFT fft;
extern LCD lcd;

extern volatile int8_t voltScale, encoderPendingL, encoderPendingR;
extern volatile int16_t vCalibOffset;
extern volatile bool Encoder1Btn, FFTMode;
extern volatile float vCalibScale;
//extern encoderType EncoderModeL, EncoderModeR;
extern volatile uint32_t debugCount, coverageTotal, coverageTimer;
extern mode displayMode;

class UI {
public:
	void DrawUI();
	void handleEncoders();
	void MenuAction(encoderType type, int8_t val);
	std::string EncoderLabel(encoderType type);
	std::string floatToString(float f, bool smartFormat);
	std::string intToString(uint16_t v);
	encoderType EncoderModeL = HorizScaleCoarse;
	encoderType EncoderModeR = VoltScale;
};


