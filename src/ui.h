#pragma once

#include <string>
#include <sstream>
#include <cmath>
#include "lcd.h"
#include "fft.h"
#include "osc.h"
#include "initialisation.h"

extern void ResetMode();

class FFT;		// forward reference to handle circular dependency
class Osc;
extern FFT fft;
extern LCD lcd;
extern Osc osc;

extern volatile int8_t voltScale, encoderPendingL, encoderPendingR;
extern volatile int16_t vCalibOffset;
extern volatile uint16_t oldAdc, capturePos, bufferSamples;
extern volatile bool Encoder1Btn, Encoder2Btn, FFTMode, capturing, drawing, menuMode;
extern volatile float vCalibScale;
extern volatile uint32_t debugCount, coverageTotal, coverageTimer;
extern mode displayMode;

enum encLR {encoderL, encoderR };

struct MenuItem {
	uint8_t pos;
	std::string name;
	encoderType selected;
	std::string val;
};

class UI {
public:
	void DrawUI();
	void handleEncoders();
	void MenuAction(encoderType* et, volatile const int8_t& val);
	void EncoderAction(encoderType type, int8_t val);
	void ResetMode();
	void DrawMenu();
	std::string EncoderLabel(encoderType type);
	std::string floatToString(float f, bool smartFormat);
	std::string intToString(uint16_t v);
	encoderType EncoderModeL, EncoderModeR;

	std::vector<MenuItem> OscMenu{  { 0, "Horiz Coarse", HorizScaleCoarse },{ 1, "Horiz Fine", HorizScaleFine },{ 2, "Vert scale", VoltScale},{ 3, "Trigger Y", TriggerY},
		{ 4, "Trigger Ch", TriggerChannel},{ 5, "Calibrate", CalibVertScale } };

};


//enum encoderType { HorizScaleCoarse, HorizScaleFine, CalibVertScale, CalibVertOffset, VoltScale, FFTAutoTune, FFTChannel };
