#pragma once

#include "initialisation.h"
#include "configManager.h"
#include <vector>

class UI {
public:
	void DrawUI();
	void handleEncoders();
	void ResetMode();


	struct Config {
		DispMode displayMode = DispMode::Oscilloscope;
	} cfg;

	ConfigSaver configSaver = {
		.settingsAddress = &cfg,
		.settingsSize = sizeof(cfg),
		.validateSettings = nullptr
	};

	std::string FloatToString(float f, bool smartFormat);
	std::string IntToString(const int32_t v);

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

	const std::vector<MenuItem> tunerMenu{
		{ 0, "Channel", ActiveChannel },
		{ 1, "Vert scale", VoltScale },
		{ 2, "Tuner Mode", TunerMode },
		{ 3, "Trace overlay", TraceOverlay } };

	char charBuff[100];

	Btn encBtnL {{GPIOB, 1, GpioPin::Type::InputPullup}};			// configure PB1 Left encoder button
	Btn encBtnR {{GPIOC, 13, GpioPin::Type::InputPullup}};		// configure PC13 right encoder button
};

extern UI ui;
