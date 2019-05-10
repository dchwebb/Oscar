#pragma once

#include <string>
#include <sstream>
#include <cmath>
#include "lcd.h"
#include "initialisation.h"

extern void ResetSampleAcquisition();

extern LCD lcd;
extern volatile int8_t voltScale, encoderPendingL, encoderPendingR;
extern volatile int16_t vCalibOffset;
extern volatile bool Encoder1Btn, FFTMode;
extern volatile float vCalibScale;
extern encoderType lEncoderMode, rEncoderMode;
extern volatile uint32_t debugCount, coverageTotal, coverageTimer;

class UI {
public:
	void DrawUI();
	std::string floatToString(float f);
	std::string intToString(uint16_t v);
	void handleEncoders();
};


