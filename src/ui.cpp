#include <ui.h>

void UI::DrawUI() {
	// Draw UI
	lcd.ColourFill(0, DRAWHEIGHT + 1, DRAWWIDTH - 1, DRAWHEIGHT + 1, LCD_GREY);
	lcd.ColourFill(0, 239, DRAWWIDTH - 1, 239, LCD_GREY);
	lcd.ColourFill(0, DRAWHEIGHT + 1, 0, 239, LCD_GREY);
	lcd.ColourFill(90, DRAWHEIGHT + 1, 90, 239, LCD_GREY);
	lcd.ColourFill(230, DRAWHEIGHT + 1, 230, 239, LCD_GREY);
	lcd.ColourFill(319, DRAWHEIGHT + 1, 319, 239, LCD_GREY);
	lcd.ColourFill(91, DRAWHEIGHT + 2, 229, 238, LCD_BLACK);

	lcd.DrawString(10, DRAWHEIGHT + 8, EncoderLabel(EncoderModeL), &lcd.Font_Small, LCD_GREY, LCD_BLACK);
	lcd.DrawString(240, DRAWHEIGHT + 8, EncoderLabel(EncoderModeR), &lcd.Font_Small, LCD_GREY, LCD_BLACK);

	if (displayMode == Oscilloscope) {
		std::string s = floatToString(640000.0f * (TIM3->PSC + 1) * (TIM3->ARR + 1) / SystemCoreClock, false) + "ms    ";
		lcd.DrawString(140, DRAWHEIGHT + 8, s, &lcd.Font_Small, LCD_WHITE, LCD_BLACK);
	}
}


void UI::MenuAction(encoderType* et, volatile const int8_t& val) {

	std::vector<MenuItem>* currentMenu;
	if (displayMode == Oscilloscope)
		currentMenu = &OscMenu;
	else if (displayMode == Fourier || displayMode == Waterfall)
		currentMenu = &FftMenu;
	else if (displayMode == Circular) {}
	else if (displayMode == MIDI) {}

	for (auto m : *currentMenu) {
		if (m.selected == *et) {
			if (val > 0 && m.pos + 1 < (uint8_t)currentMenu->size()) {
				*et = (*currentMenu)[m.pos + 1].selected;
			} else if (val < 0 && m.pos > 0) {
				*et = (*currentMenu)[m.pos - 1].selected;
			}
			break;
		}
	}
	if (displayMode == Oscilloscope) {
		osc.EncModeL = EncoderModeL;
		osc.EncModeR = EncoderModeR;
	} else if (displayMode == Fourier) {
		fft.EncModeL = EncoderModeL;
		fft.EncModeR = EncoderModeR;
	}

	DrawMenu();
}

void UI::EncoderAction(encoderType type, int8_t val) {
	int16_t adj;
	switch (type) {
	case HorizScaleCoarse :
		adj = TIM3->ARR + 10 * val;
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
	case TriggerChannel :
		osc.TriggerChannel = (osc.TriggerChannel == channelA) ? channelB : (osc.TriggerChannel == channelB) ? channelNone: channelA;
		switch (osc.TriggerChannel) {
		case channelA :
			osc.TriggerTest = &adcA;
			break;
		case channelB :
			osc.TriggerTest = &adcB;
			break;
		default :
			break;
		}
		DrawUI();
		break;
	case TriggerY :
		osc.TriggerY += 100 * val;
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
void UI::DrawMenu() {

	lcd.DrawString(10, 6, "L", &lcd.Font_Large, LCD_WHITE, LCD_BLACK);
	lcd.DrawString(80, 6, "Encoder Action", &lcd.Font_Large, LCD_ORANGE, LCD_BLACK);
	lcd.DrawString(303, 6, "R", &lcd.Font_Large, LCD_WHITE, LCD_BLACK);
	lcd.DrawRect(0, 1, 319, 239, LCD_WHITE);
	lcd.DrawLine(0, 27, 319, 27, LCD_WHITE);
	lcd.DrawLine(26, 1, 26, 27, LCD_WHITE);
	lcd.DrawLine(294, 1, 294, 27, LCD_WHITE);
	lcd.DrawLine(159, 27, 159, 239, LCD_WHITE);

	std::vector<MenuItem>* currentMenu;
	if (displayMode == Oscilloscope)
		currentMenu = &OscMenu;
	else if (displayMode == Fourier || displayMode == Waterfall)
		currentMenu = &FftMenu;

	uint8_t pos = 0;
	for (auto m = currentMenu->cbegin(); m != currentMenu->cend(); m++, pos++) {
		lcd.DrawString(10, 32 + pos * 20, m->name, &lcd.Font_Large, (m->selected == EncoderModeL) ? LCD_BLACK : LCD_WHITE, (m->selected == EncoderModeL) ? LCD_WHITE : LCD_BLACK);
		lcd.DrawString(170, 32 + pos * 20, m->name, &lcd.Font_Large, (m->selected == EncoderModeR) ? LCD_BLACK : LCD_WHITE, (m->selected == EncoderModeR) ? LCD_WHITE : LCD_BLACK);
	}

/*
	for (uint8_t m = 0; m < OscMenu.size(); ++m) {
		lcd.DrawString(10, 32 + m * 20, OscMenu[m].name, &lcd.Font_Large, (OscMenu[m].selected == EncoderModeL) ? LCD_BLACK : LCD_WHITE, (OscMenu[m].selected == EncoderModeL) ? LCD_WHITE : LCD_BLACK);
		lcd.DrawString(170, 32 + m * 20, OscMenu[m].name, &lcd.Font_Large, (OscMenu[m].selected == EncoderModeR) ? LCD_BLACK : LCD_WHITE, (OscMenu[m].selected == EncoderModeR) ? LCD_WHITE : LCD_BLACK);

		//if (OscMenu[m].selected == EncoderModeL) 	lcd.DrawString(0, 25 + m * 20, "*", &lcd.Font_Large, LCD_WHITE, LCD_BLACK);
		//if (OscMenu[m].selected == EncoderModeR) 	lcd.DrawString(160, 25 + m * 20, "*", &lcd.Font_Large, LCD_WHITE, LCD_BLACK);
	}
*/
}

void UI::handleEncoders() {
	if (encoderPendingL) {
		if (menuMode)	MenuAction(&EncoderModeL, encoderPendingL);
		else			EncoderAction(EncoderModeL, encoderPendingL);

		encoderPendingL = 0;

	}

	if (encoderPendingR) {
		if (menuMode)	MenuAction(&EncoderModeR, encoderPendingR);
		else			EncoderAction(EncoderModeR, encoderPendingR);

		encoderPendingR = 0;
	}

	if ((encoderBtnL || encoderBtnR) && menuMode) {
		encoderBtnL = encoderBtnR = menuMode = false;
		DrawUI();
		return;
	}

	// Menu mode
	if (encoderBtnL) {
		encoderBtnL = false;
		menuMode = true;
		lcd.ScreenFill(LCD_BLACK);
		DrawMenu();
	}

	// Change display mode
	if (encoderBtnR) {
		encoderBtnR = false;

		switch (displayMode) {
		case Oscilloscope :	displayMode = Fourier;			break;
		case Fourier :		displayMode = Waterfall;		break;
		case Waterfall :	displayMode = Circular;			break;
		case Circular :		displayMode = MIDI;				break;
		case MIDI :			displayMode = Oscilloscope;		break;
		}
		ResetMode();
	}
}

void UI::ResetMode() {
	TIM3->CR1 &= ~TIM_CR1_CEN;				// Disable the sample acquisiton timer
	UART4->CR1 &= ~USART_CR1_UE;			// Disable MIDI capture on UART4

	lcd.ScreenFill(LCD_BLACK);
	switch (displayMode) {
	case Oscilloscope :
		EncoderModeL = osc.EncModeL;
		EncoderModeR = osc.EncModeR;
		break;
	case Fourier :
		EncoderModeL = fft.EncModeL;
		EncoderModeR = fft.EncModeR;
		break;
	case Waterfall :
		EncoderModeL = HorizScaleCoarse;
		EncoderModeR = FFTChannel;
		break;
	case Circular :
		EncoderModeL = FFTChannel;
		EncoderModeR = VoltScale;
		break;
	case MIDI :
		break;
	}
	ui.DrawUI();

	capturing = drawing = false;
	bufferSamples = capturePos = oldAdc = 0;
	fft.dataAvailable[0] = fft.dataAvailable[1] = false;
	fft.samples = displayMode == Fourier ? FFTSAMPLES : WATERFALLSAMPLES;

	if (displayMode == MIDI) {
		UART4->CR1 |= USART_CR1_UE;			// Enable MIDI capture
	} else {
		TIM3->CR1 |= TIM_CR1_CEN;			// Reenable the sample acquisiton timer
	}
}


std::string UI::EncoderLabel(encoderType type) {
	switch (type) {
	case HorizScaleCoarse :
		return "Zoom Horiz";
	case HorizScaleFine :
		return "Zoom Horiz";
	case CalibVertScale :
		return "Calib Scale";
	case CalibVertOffset :
		return "Calib Offset";
	case VoltScale :
		return "Zoom Vert";
	case TriggerY :
		return "Trigger Y";
	case TriggerChannel :
		return std::string(osc.TriggerChannel == channelA ? "Trigger A " : osc.TriggerChannel == channelB ? "Trigger B " : "No Trigger");
	case FFTAutoTune :
		return "Tune: " + std::string(fft.autoTune ? "auto" : "off ");
	case FFTChannel :
		return "Channel " + std::string(fft.channel == channelA ? "A" : "B");
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
