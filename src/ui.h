#pragma once

#include <string>
#include <sstream>
#include <cmath>
#include "lcd.h"

extern Lcd lcd;

class ui {
public:
	void DrawUI();
	std::string floatToString(float f);
	std::string intToString(uint16_t v);

};


