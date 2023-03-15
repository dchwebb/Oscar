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




class UI {
public:
	void DrawUI();
	void handleEncoders();
	void ResetMode();

	std::string FloatToString(float f, bool smartFormat);
	std::string IntToString(const uint16_t v);
	uint16_t DarkenColour(const uint16_t& colour, uint16_t amount);

	bool menuMode = false, encoderBtnL = false, encoderBtnR = false;

private:
	void DrawMenu();
	void MenuAction(encoderType* et, volatile const int8_t& val);
	void EncoderAction(encoderType type, const int8_t& val);
	std::string EncoderLabel(encoderType type);

	encoderType encoderModeL, encoderModeR;

	struct MenuItem {
		int8_t pos;
		std::string name;
		encoderType selected;
		std::string val;
	};

	const std::vector<MenuItem> oscMenu{
		{ 0, "Horiz scale", HorizScale },
		{ 1, "Vert scale", VoltScale },
		{ 2, "Channel Sel", ChannelSelect },
		{ 3, "Multi-Lane", MultiLane },
		{ 4, "Trigger Y", Trigger_Y },
		{ 5, "Trigger X", Trigger_X },
		{ 6, "Trigger Ch", TriggerChannel },
		{ 7, "Calib Scale", CalibVertScale },
		{ 8, "Calib Offset", CalibVertOffset } };

	const std::vector<MenuItem> fftMenu{
		{ 0, "Horiz scale", HorizScale },
		{ 1, "Vert scale", VoltScale },
		{ 2, "Channel", ActiveChannel },
		{ 3, "AutoTune", FFTAutoTune },
		{ 4, "Trace overlay", TraceOverlay } };

	const std::vector<MenuItem> circMenu{
		{ 0, "Vert scale", VoltScale },
		{ 1, "Channel", ActiveChannel },
		{ 2, "Zero cross", ZeroCross } };

	char charBuff[100];
};

