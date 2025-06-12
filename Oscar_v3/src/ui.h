#pragma once

#include "initialisation.h"
#include "configManager.h"
#include <vector>

enum EncoderType { HorizScale, CalibVertScale, CalibVertOffset, VertScale, TriggerChannel, Trigger_X, Trigger_Y,
	FFTAutoTune, ActiveChannel, MultiLane, TraceOverlay, TunerMode, ReverseEncoders };


class UI {
public:
	void DrawUI();
	void handleEncoders();
	void ResetMode();
	std::string FloatToString(float f, bool smartFormat);
	std::string IntToString(const int32_t v);

	struct Config {
		DispMode displayMode = DispMode::Oscilloscope;
		bool reverseEncoders = false;
	} cfg;

	ConfigSaver configSaver = {
		.settingsAddress = &cfg,
		.settingsSize = sizeof(cfg),
		.validateSettings = nullptr
	};

	enum class MenuMode { off, encoder, system } menuMode;

	Btn btnEncL {{GPIOB, 1, GpioPin::Type::InputPullup}};
	Btn btnEncR {{GPIOC, 13, GpioPin::Type::InputPullup}};
	Btn btnMenu {{GPIOA, 2, GpioPin::Type::InputPullup}};

private:
	void DrawMenu();
	void DrawSystemMenu();
	void MenuAction(EncoderType& et, const int8_t val);
	void SysMenuAction(int8_t v, bool position);
	void EncoderAction(EncoderType type, const int8_t val);
	std::string_view EncoderLabel(EncoderType type);

	EncoderType encoderModeL, encoderModeR;
	int8_t sysMenuPos = 0;

	struct MenuItem {
		int8_t pos;
		std::string name;
		EncoderType selected;
		std::string val;
	};

	const std::vector<MenuItem> oscMenu {
		{ 0, "Horiz scale", HorizScale },
		{ 1, "Vert scale", VertScale },
		{ 2, "Multi-Lane", MultiLane },
		{ 3, "Trigger Y", Trigger_Y },
		{ 4, "Trigger X", Trigger_X },
		{ 5, "Trigger Ch", TriggerChannel },
		{ 6, "Calib Scale", CalibVertScale },
		{ 7, "Calib Offset", CalibVertOffset } };

	const std::vector<MenuItem> fftMenu {
		{ 0, "Vert scale", VertScale },
		{ 1, "AutoTune", FFTAutoTune },
		{ 2, "Horiz scale", HorizScale },
		{ 3, "Trace overlay", TraceOverlay } };

	const std::vector<MenuItem> tunerMenu {
		{ 0, "Vert scale", VertScale },
		{ 1, "Tuner Mode", TunerMode },
		{ 2, "Trace overlay", TraceOverlay } };

	const std::vector<MenuItem> systemMenu {
		{ 0, "Reverse Encoders", ReverseEncoders } };

	char charBuff[100];

	struct {
		Btn btnChA {{GPIOB, 14, GpioPin::Type::InputPullup}};
		Btn btnChB {{GPIOB, 0, GpioPin::Type::InputPullup}};
		Btn btnChC {{GPIOA, 3, GpioPin::Type::InputPullup}};

		GpioPin ledChA {GPIOB, 13, GpioPin::Type::Output};
		GpioPin ledChB {GPIOB, 12, GpioPin::Type::Output};
		GpioPin ledChC {GPIOB, 15, GpioPin::Type::Output};
	} channelSelect;
};

extern UI ui;
