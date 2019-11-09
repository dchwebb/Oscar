#pragma once

#include "initialisation.h"
#include "lcd.h"
#include "fft.h"
#include "osc.h"
#include "config.h"

extern void ResetMode();

class FFT;		// forward reference to handle circular dependency
class Osc;
class Config;
extern FFT fft;
extern LCD lcd;
extern Osc osc;
extern Config cfg;

extern volatile int16_t vCalibOffset;
extern volatile float vCalibScale;
extern volatile uint16_t oldAdc, capturePos, adcA, adcB, adcC;
extern volatile bool drawing;
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
	uint16_t DarkenColour(const uint16_t& colour, uint16_t amount);

	encoderType EncoderModeL, EncoderModeR;
	bool menuMode = false, encoderBtnL = false, encoderBtnR = false;

	std::vector<MenuItem> OscMenu{  { 0, "Horiz scale", HorizScale },
									{ 1, "Vert scale", VoltScale},
									{ 2, "Channel Sel", ChannelSelect },
									{ 3, "Multi-Lane", MultiLane },
									{ 4, "Trigger Y", Trigger_Y},
									{ 5, "Trigger X", Trigger_X},
									{ 6, "Trigger Ch", TriggerChannel},
									{ 7, "Calib Scale", CalibVertScale },
									{ 8, "Calib Offset", CalibVertOffset } };

	std::vector<MenuItem> FftMenu{  { 0, "Horiz scale", HorizScale },
									{ 1, "AutoTune", FFTAutoTune},
									{ 2, "Trace overlay", TraceOverlay},
									{ 3, "Multi-Lane", MultiLane } };

	std::vector<MenuItem> CircMenu{ { 0, "Vert scale", VoltScale},
									{ 1, "Channel", ActiveChannel},
									{ 2, "Zero cross", ZeroCross } };

};

