#pragma once

#include "stm32f4xx.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <Array>
#include <string>
#include "GpioPin.h"

static constexpr uint32_t sysTickInterval = 1000;


#define ADC_BUFFER_LENGTH 12
#define MINSAMPLETIMER 200
extern float samplingFrequency;

enum encoderType { HorizScale, HorizScaleFine, CalibVertScale, CalibVertOffset, VoltScale, TriggerChannel, Trigger_X, Trigger_Y,
	FFTAutoTune, ActiveChannel, ChannelSelect, MultiLane, TraceOverlay, TunerMode };

enum class DispMode { Oscilloscope, Tuner, Fourier, Waterfall, MIDI };
enum oscChannel {channelA = 0, channelB = 1, channelC = 2, channelNone = 3};

struct ADCValues {
	uint16_t ChA_1;
	uint16_t ChB_1;
	uint16_t ChC_1;
	uint16_t ChA_2;
	uint16_t ChB_2;
	uint16_t ChC_2;
	uint16_t ChA_3;
	uint16_t ChB_3;
	uint16_t ChC_3;
	uint16_t ChA_4;
	uint16_t ChB_4;
	uint16_t ChC_4;
};
extern volatile ADCValues adc;

extern volatile uint32_t SysTickVal;
extern GpioPin debugPin;

void InitHardware();
void InitClocks(void);
void InitBackupRAM();
void InitSysTick();
void InitWatchdog();
void ResetWatchdog();
void InitLCDHardware(void);
void InitADC(void);
void InitSampleAcquisition();
void SetSampleTimer(uint32_t val);
void InitEncoders();
void InitMIDIUART();
void DelayMS(uint32_t ms);
void JumpToBootloader();
