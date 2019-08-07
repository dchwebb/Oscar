#pragma once

#include "initialisation.h"
#include "lcd.h"
#include "fft.h"
#include "osc.h"

extern void ResetMode();

class FFT;		// forward reference to handle circular dependency
class Osc;
extern FFT fft;
extern LCD lcd;
extern Osc osc;

//extern volatile int8_t voltScale;
extern volatile int16_t vCalibOffset;
extern volatile float vCalibScale;
extern volatile uint16_t oldAdc, capturePos, bufferSamples, adcA, adcB, adcC;
extern volatile bool encoderBtnL, encoderBtnR, capturing, drawing, menuMode;
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
	void EncoderAction(encoderType type, const int8_t& val);
	void ResetMode();
	void DrawMenu();
	std::string EncoderLabel(encoderType type);
	std::string floatToString(float f, bool smartFormat);
	std::string intToString(uint16_t v);
	encoderType EncoderModeL, EncoderModeR;
	bool menuMode = false;

	std::vector<MenuItem> OscMenu{  { 0, "Horiz Coarse", HorizScaleCoarse },{ 1, "Horiz Fine", HorizScaleFine },{ 2, "Vert scale", VoltScale},{ 3, "Trigger Y", Trigger_Y},
		{ 4, "Trigger Ch", TriggerChannel},{ 5, "Calib Scale", CalibVertScale },{ 5, "Calib Offset", CalibVertOffset },{ 6, "Channel Sel", ChannelSelect } };
	std::vector<MenuItem> FftMenu{  { 0, "Horiz Coarse", HorizScaleCoarse },{ 1, "Horiz Fine", HorizScaleFine },{ 2, "AutoTune", FFTAutoTune},{ 3, "Channel", FFTChannel} };
	std::vector<MenuItem> CircMenu{ { 0, "Vert scale", VoltScale}, { 1, "Channel", FFTChannel},{ 2, "Zero cross", ZeroCross } };


};


//enum encoderType { HorizScaleCoarse, HorizScaleFine, CalibVertScale, CalibVertOffset, VoltScale, FFTAutoTune, FFTChannel };
