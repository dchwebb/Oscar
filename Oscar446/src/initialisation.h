#pragma once

#include "stm32f4xx.h"
#include <cmath>
#include <string>
#include <cstring>
#include <array>
#include <vector>
#include <algorithm>

// Coverage profiler macros using timer 4 to count clock cycles / 10
#define CP_ON		TIM9->EGR |= TIM_EGR_UG; TIM9->CR1 |= TIM_CR1_CEN; coverageTimer = 0;
#define CP_OFF		TIM9->CR1 &= ~TIM_CR1_CEN;
#define CP_CAP		TIM9->CR1 &= ~TIM_CR1_CEN; coverageTotal = (coverageTimer * 65536) + TIM9->CNT;


#define ADC_BUFFER_LENGTH 12
#define MINSAMPLETIMER 200

enum encoderType { HorizScale, HorizScaleFine, CalibVertScale, CalibVertOffset, VoltScale, TriggerChannel, Trigger_X, Trigger_Y,
	FFTAutoTune, ActiveChannel, ChannelSelect, MultiLane, TraceOverlay, TunerMode };

enum class DispMode { Oscilloscope, Tuner, Fourier, Waterfall, MIDI };
enum oscChannel {channelA, channelB, channelC, channelNone};

extern volatile uint16_t ADC_array[];
extern volatile uint32_t SysTickVal;
extern uint32_t coverageTotal, coverageTimer;



void SystemClock_Config(void);
void InitSysTick();
void InitLCDHardware(void);
void InitADC(void);
void InitSampleAcquisition();
void InitCoverageTimer();
void InitEncoders();
void InitUART();

