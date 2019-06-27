#pragma once

#include <string>
#include <sstream>
#include <cmath>
#include <algorithm>
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

extern volatile int8_t voltScale, encoderPendingL, encoderPendingR, encoderStateL;
extern volatile int16_t vCalibOffset;
extern volatile uint16_t oldAdc, capturePos, bufferSamples, adcA, adcB, adcC;
extern volatile bool encoderBtnL, encoderBtnR, FFTMode, capturing, drawing, menuMode;
extern volatile float vCalibScale;
extern volatile uint32_t debugCount, coverageTotal, coverageTimer;
extern mode displayMode;

struct MenuItem {
	int8_t pos;
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
	bool menuMode = false;

	std::vector<MenuItem> OscMenu{  { 0, "Horiz Coarse", HorizScaleCoarse },{ 1, "Horiz Fine", HorizScaleFine },{ 2, "Vert scale", VoltScale},{ 3, "Trigger Y", TriggerY},
		{ 4, "Trigger Ch", TriggerChannel},{ 5, "Calib Scale", CalibVertScale },{ 5, "Calib Offset", CalibVertOffset } };
	std::vector<MenuItem> FftMenu{  { 0, "Horiz Coarse", HorizScaleCoarse },{ 1, "Horiz Fine", HorizScaleFine },{ 2, "AutoTune", FFTAutoTune},{ 3, "Channel", FFTChannel} };


};


//enum encoderType { HorizScaleCoarse, HorizScaleFine, CalibVertScale, CalibVertOffset, VoltScale, FFTAutoTune, FFTChannel };
