#pragma once

#include "initialisation.h"
#include "lcd.h"
#include "ui.h"


class UI;		// forward reference to handle circular dependency
extern UI ui;
extern LCD lcd;


class Tuner {
public:
	encoderType encModeL = ZeroCrossing;
	encoderType encModeR = ActiveChannel;
};

extern Tuner tuner;
