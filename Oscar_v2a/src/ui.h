#pragma once

#include "initialisation.h"
#include <vector>

class UI {
public:
	void DrawUI();
	void handleEncoders();
	void ResetMode();
	uint32_t SerialiseConfig(uint8_t** buff);
	uint32_t StoreConfig(uint8_t* buff);

	struct Config {
		DispMode displayMode = DispMode::Oscilloscope;
	} config;


	std::string FloatToString(float f, bool smartFormat);
	std::string IntToString(const int32_t v);
	uint16_t DarkenColour(const uint16_t& colour, uint16_t amount);

	bool menuMode = false, encoderBtnL = false, encoderBtnR = false;

private:
	void DrawMenu();
	void MenuAction(encoderType* et, volatile const int8_t& val);
	void EncoderAction(encoderType type, const int8_t& val);
	std::string EncoderLabel(encoderType type);

	encoderType encoderModeL, encoderModeR;
	uint32_t leftBtnReleased = 0;		// Debounce counters
	uint32_t rightBtnReleased = 0;

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

	const std::vector<MenuItem> tunerMenu{
		{ 0, "Channel", ActiveChannel },
		{ 1, "Vert scale", VoltScale },
		{ 2, "Tuner Mode", TunerMode },
		{ 3, "Trace overlay", TraceOverlay } };

	char charBuff[100];
};

extern UI ui;
