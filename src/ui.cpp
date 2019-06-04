#include <ui.h>

void UI::DrawUI() {
	// Draw UI
	lcd.ColourFill(0, DRAWHEIGHT + 1, DRAWWIDTH - 1, DRAWHEIGHT + 1, LCD_GREY);
	lcd.ColourFill(0, 239, DRAWWIDTH - 1, 239, LCD_GREY);
	lcd.ColourFill(0, DRAWHEIGHT + 1, 0, 239, LCD_GREY);
	lcd.ColourFill(90, DRAWHEIGHT + 1, 90, 239, LCD_GREY);
	lcd.ColourFill(230, DRAWHEIGHT + 1, 230, 239, LCD_GREY);
	lcd.ColourFill(319, DRAWHEIGHT + 1, 319, 239, LCD_GREY);

	lcd.DrawString(10, DRAWHEIGHT + 8, EncoderLabel(EncoderModeL), &lcd.Font_Small, LCD_GREY, LCD_BLACK);
	lcd.DrawString(240, DRAWHEIGHT + 8, EncoderLabel(EncoderModeR), &lcd.Font_Small, LCD_GREY, LCD_BLACK);

	if (displayMode == Oscilloscope) {
		std::string s = floatToString(640000.0f * (TIM3->PSC + 1) * (TIM3->ARR + 1) / SystemCoreClock, false) + "ms    ";
		lcd.DrawString(140, DRAWHEIGHT + 8, s, &lcd.Font_Small, LCD_WHITE, LCD_BLACK);
	}
}


void UI::MenuAction(encoderType type, int8_t val) {
	int16_t adj;
	switch (type) {
	case HorizScaleCoarse :
		adj = TIM3->ARR + 50 * val;
		if (adj > 0 && adj < 6000)
			TIM3->ARR = adj;
		DrawUI();
		break;
	case HorizScaleFine :
		TIM3->ARR += val;
		DrawUI();
		break;
	case CalibVertOffset :
		vCalibOffset += 50 * val;
		break;
	case CalibVertScale :
		vCalibScale += val * .01;
		break;
	case VoltScale :
		voltScale += val;
		voltScale = std::max(std::min((int)voltScale, 8), 1);
		if (displayMode == Circular) {
			lcd.ScreenFill(LCD_BLACK);
			DrawUI();
		}
		break;
	case FFTAutoTune :
		fft.autoTune = !fft.autoTune;
		DrawUI();
		break;
	case FFTChannel :
		fft.channel = fft.channel == channelA? channelB : channelA;
		DrawUI();
		break;
	default:
	  break;
	}
}

void UI::handleEncoders() {
	if (encoderPendingL && (GPIOE->IDR & GPIO_IDR_IDR_11) && (GPIOE->IDR & GPIO_IDR_IDR_10)) {
		MenuAction(EncoderModeL, encoderPendingL);
		encoderPendingL = 0;
	}

	if (encoderPendingR && (GPIOE->IDR & GPIO_IDR_IDR_8) && (GPIOE->IDR & GPIO_IDR_IDR_9)) {
		MenuAction(EncoderModeR, encoderPendingR);
		encoderPendingR = 0;
	}

	if (Encoder1Btn) {
		Encoder1Btn = false;
		switch (displayMode) {
		case Oscilloscope :
			displayMode = Fourier;
			break;
		case Fourier :
			displayMode = Waterfall;
			break;
		case Waterfall :
			displayMode = Circular;
			break;
		case Circular :
			displayMode = Oscilloscope;
			break;
		}
	ResetMode();
	}
}

std::string UI::EncoderLabel(encoderType type) {
	switch (type) {
	case HorizScaleCoarse :
		return "Zoom Horiz";
	case HorizScaleFine :
		return "Zoom Horiz";
	case CalibVertScale :
		return "V Calib";
	case CalibVertOffset :
		return "Vert Offset";
	case VoltScale :
		return "Zoom Vert";
	case FFTAutoTune :
		return "Tune: " + std::string(fft.autoTune ? "auto" : "off ");
	case FFTChannel :
		return "Channel " + std::string (fft.channel == channelA ? "A" : "B");
	default:
	  return "";
	}
}


std::string UI::floatToString(float f, bool smartFormat) {
	std::string s;
	std::stringstream ss;

	if (smartFormat && f > 10000) {
		ss << (int16_t)std::round(f / 100);
		s = ss.str();
		s.insert(s.length() - 1, ".");
		s+= "k";
	} else if (smartFormat && f > 1000) {
		ss << (int16_t)std::round(f);
		s = ss.str();
	} else	{
		ss << (int16_t)std::round(f * 10);
		s = ss.str();
		s.insert(s.length() - 1, ".");
	}
	return s;
}


std::string UI::intToString(uint16_t v) {
	std::stringstream ss;
	ss << v;
	return ss.str();
}
