#pragma once

#include "stm32f4xx.h"

#define USE_HSI
#define PLL_M 8
#define PLL_N 180
#define PLL_P 0		//  Main PLL (PLL) division factor for main system clock can be 2 (PLL_P = 0), 4 (PLL_P = 1), 6 (PLL_P = 2), 8 (PLL_P = 3)
#define PLL_Q 0
#define AHB_PRESCALAR 0b0000
#define APB1_PRESCALAR 0b100		// AHB divided by 2 APB Prescaler: 0b0xx: AHB clock not divided, 0b100 div by  2, 0b101: div by  4, 0b110 div by  8; 0b111 div by 16
#define APB2_PRESCALAR 0b100

// Coverage profiler macros using timer 4 to count clock cycles / 10
#define CP_ON		TIM4->EGR |= TIM_EGR_UG;TIM4->CR1 |= TIM_CR1_CEN;coverageTimer=0;
#define CP_OFF		TIM4->CR1 &= ~TIM_CR1_CEN;
#define CP_CAP		TIM4->CR1 &= ~TIM_CR1_CEN;coverageTotal = (coverageTimer * 65536) + TIM4->CNT;

#define ADC_BUFFER_LENGTH 8

extern volatile uint16_t ADC_array[];
enum encoderType { HorizScaleCoarse, HorizScaleFine, CalibVertScale, CalibVertOffset, VoltScale, FFTAutoTune, FFTChannel };
enum mode { Oscilloscope, Fourier, Waterfall, Circular };

void SystemClock_Config(void);
void InitSysTick();
void InitLCDHardware(void);
void InitADC(void);
void InitSampleAcquisition();
void InitCoverageTimer();
void InitEncoders();
