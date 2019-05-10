#include <ui.h>

void UI::DrawUI() {
	// Draw UI
	lcd.ColourFill(0, DRAWHEIGHT + 1, DRAWWIDTH - 1, DRAWHEIGHT + 1, LCD_GREY);
	lcd.ColourFill(0, 239, DRAWWIDTH - 1, 239, LCD_GREY);
	lcd.ColourFill(0, DRAWHEIGHT + 1, 0, 239, LCD_GREY);
	lcd.ColourFill(90, DRAWHEIGHT + 1, 90, 239, LCD_GREY);
	lcd.ColourFill(230, DRAWHEIGHT + 1, 230, 239, LCD_GREY);
	lcd.ColourFill(319, DRAWHEIGHT + 1, 319, 239, LCD_GREY);

	std::string label;

	CP_ON

	switch (lEncoderMode) {
	case HorizScaleCoarse :
		label = "Zoom Horiz";
		break;
	case HorizScaleFine :
		label = "Zoom Horiz";
	break;
	case CalibVertScale :
		label = "V Calib";
		break;
	case VoltScale :
		label = "Zoom Vert";
	default:
	  break;
	}
	lcd.DrawString(10, DRAWHEIGHT + 8, label, &lcd.Font_Small, LCD_GREY, LCD_BLACK);

	switch (rEncoderMode) {
	case HorizScaleCoarse :
		label = "Zoom Horiz";
		break;
	case CalibVertOffset :
		label = "Vert Offset";
		break;
	case VoltScale :
		label = "Zoom Vert";
		break;
	default:
	  break;
	}
	lcd.DrawString(240, DRAWHEIGHT + 8, label, &lcd.Font_Small, LCD_GREY, LCD_BLACK);

	std::string s = floatToString(640000.0f * (TIM3->PSC + 1) * (TIM3->ARR + 1) / SystemCoreClock) + "ms    ";
	lcd.DrawString(140, DRAWHEIGHT + 8, s, &lcd.Font_Small, LCD_WHITE, LCD_BLACK);

	CP_CAP
}

std::string UI::floatToString(float f) {
	std::stringstream ss;
	ss << (int16_t)std::round(f * 10);
	std::string s = ss.str();
	s.insert(s.length() - 1, ".");
	return s;
}

std::string UI::intToString(uint16_t v) {
	std::stringstream ss;
	ss << v;
	return ss.str();
}

void UI::handleEncoders() {
	if (encoderPendingL && (GPIOE->IDR & GPIO_IDR_IDR_11) && (GPIOE->IDR & GPIO_IDR_IDR_10)) {
		int16_t adj;
		switch (lEncoderMode) {
		case HorizScaleCoarse :
			adj = TIM3->ARR + 50 * encoderPendingL;
			if (adj > 0 && adj < 6000)	TIM3->ARR = adj;
			encoderPendingL = 0;
			DrawUI();
			break;
		case HorizScaleFine :
			TIM3->ARR += encoderPendingL;
			encoderPendingL = 0;
			DrawUI();
			break;
		case CalibVertScale :
			vCalibScale += encoderPendingL * .01;
			encoderPendingL = 0;
			break;
		default:
		  break;
		}
	}

	if (encoderPendingR && (GPIOE->IDR & GPIO_IDR_IDR_8) && (GPIOE->IDR & GPIO_IDR_IDR_9)) {
		switch (rEncoderMode) {
		case CalibVertOffset :
			vCalibOffset += 50 * encoderPendingR;
			encoderPendingR = 0;
			break;
		case VoltScale :
			voltScale += encoderPendingR;
			voltScale = std::max(std::min((int)voltScale, 8), 1);
			encoderPendingR = 0;
			break;
		default:
		  break;
		}
	}


	if (Encoder1Btn) {
		Encoder1Btn = false;
		FFTMode = !FFTMode;
		ResetSampleAcquisition();
	}
}
