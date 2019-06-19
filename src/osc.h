#pragma once

#include "initialisation.h"

extern volatile uint16_t adcA, adcB;

class Osc {
public:
	uint16_t TriggerX = 10;
	uint16_t TriggerY = 9000;
	//oscChannel TriggerChannel = channelA;
	volatile uint16_t* TriggerTest = &adcA;		// store the currently active trigger channel as a reference for faster interrupt performance
	encoderType EncModeL = HorizScaleCoarse;
	encoderType EncModeR = VoltScale;
	float Freq;

};

