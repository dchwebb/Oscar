#include <ui.h>

void UI::DrawUI()
{
	if (displayMode == MIDI) {
		lcd.DrawString(120, lcd.drawHeight + 8, "MIDI Events", &lcd.Font_Small, LCD_GREY, LCD_BLACK);
		return;
	}

	// Draw UI
	lcd.DrawRect(0, lcd.drawHeight + 1, 319, 239, LCD_GREY);
	lcd.ColourFill(90, lcd.drawHeight + 1, 90, 239, LCD_GREY);
	lcd.ColourFill(230, lcd.drawHeight + 1, 230, 239, LCD_GREY);

	lcd.ColourFill(91, lcd.drawHeight + 2, 229, 238, LCD_BLACK);

	lcd.DrawString(10, lcd.drawHeight + 8, EncoderLabel(EncoderModeL), &lcd.Font_Small, LCD_GREY, LCD_BLACK);
	lcd.DrawString(240, lcd.drawHeight + 8, EncoderLabel(EncoderModeR), &lcd.Font_Small, LCD_GREY, LCD_BLACK);

	if (displayMode == Oscilloscope) {
		std::string s = floatToString(640000.0f * (TIM3->PSC + 1) * (TIM3->ARR + 1) / SystemCoreClock, false) + "ms    ";
		lcd.DrawString(140, lcd.drawHeight + 8, s, &lcd.Font_Small, LCD_WHITE, LCD_BLACK);
	}
}


void UI::MenuAction(encoderType* et, volatile const int8_t& val) {

	std::vector<MenuItem>* currentMenu;
	if (displayMode == Oscilloscope)
		currentMenu = &OscMenu;
	else if (displayMode == Fourier || displayMode == Waterfall)
		currentMenu = &FftMenu;
	else if (displayMode == Circular)
		currentMenu = &CircMenu;
	else if (displayMode == MIDI) {}

	//	Move the selected menu item one forwards or one back based on value of encoder
	auto mi = std::find_if(currentMenu->cbegin(), currentMenu->cend(), [=] (MenuItem m) { return m.selected == *et; } );
	if ((mi != currentMenu->cbegin() && val < 0) || (mi != currentMenu->cend() - 1 && val > 0)) {
		mi += val;
		*et = mi->selected;
	}


	if (displayMode == Oscilloscope) {
		osc.encModeL = EncoderModeL;
		osc.encModeR = EncoderModeR;
	} else if (displayMode == Circular) {
		osc.circEncModeL = EncoderModeL;
		osc.circEncModeR = EncoderModeR;
	} else if (displayMode == Fourier) {
		fft.EncModeL = EncoderModeL;
		fft.EncModeR = EncoderModeR;
	}

	DrawMenu();
}


void UI::EncoderAction(encoderType type, const int8_t& val) {

	int16_t adj;
	switch (type) {
	case HorizScale :
		adj = TIM3->ARR + (TIM3->ARR < 5000 ? 200 : TIM3->ARR < 20000 ? 400 : TIM3->ARR < 50000 ? 4000 : 8000) * -val;
		if (adj > MINSAMPLETIMER && adj < 560000) {
			TIM3->ARR = adj;
			if (displayMode == Oscilloscope)		osc.sampleTimer = adj;
			DrawUI();
		}
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
	case ZeroCross :
		osc.CircZeroCrossings = std::max(std::min(osc.CircZeroCrossings + val, 8), 1);
		DrawUI();
		break;
	case VoltScale :
		osc.voltScale -= val;
		osc.voltScale = std::max(std::min((int)osc.voltScale, 12), 1);
		if (displayMode == Circular) {
			lcd.ScreenFill(LCD_BLACK);
			DrawUI();
		}
		break;
	case ChannelSelect :
		osc.oscDisplay += val;
		osc.oscDisplay = osc.oscDisplay == 0 ? 7 : osc.oscDisplay == 8 ? 1 : osc.oscDisplay;
		osc.setTriggerChannel();

		DrawUI();
		break;

	case TriggerChannel :
		if ((osc.triggerChannel == channelNone && val > 0) || (osc.triggerChannel == channelB && val < 0))
			osc.triggerChannel = channelA;
		else if ((osc.triggerChannel == channelA && val > 0) || (osc.triggerChannel == channelC && val < 0))
			osc.triggerChannel = channelB;
		else if ((osc.triggerChannel == channelB && val > 0) || (osc.triggerChannel == channelNone && val < 0))
			osc.triggerChannel = channelC;
		else if ((osc.triggerChannel == channelC && val > 0) || (osc.triggerChannel == channelA && val < 0))
			osc.triggerChannel = channelNone;

		osc.setTriggerChannel();

		DrawUI();
		break;
	case Trigger_Y :
		osc.triggerY = std::min(std::max((int32_t)osc.triggerY + 100 * val, (int32_t)3800), (int32_t)16000);
		break;
	case Trigger_X :
		osc.triggerX = std::min(std::max(osc.triggerX + 2 * val, 0), 316);
		break;
	case FFTAutoTune :
		fft.autoTune = !fft.autoTune;
		DrawUI();
		break;
	case TraceOverlay :
		fft.traceOverlay = !fft.traceOverlay;
		DrawUI();
		break;
	case ActiveChannel :
		if (val > 0)
			fft.channel = (fft.channel == channelA) ? channelB : (fft.channel == channelB) ? channelC : channelA;
		else
			fft.channel = (fft.channel == channelA) ? channelC : (fft.channel == channelB) ? channelA : channelB;
		DrawUI();
		break;
	case MultiLane :
		osc.multiLane = !osc.multiLane;
		DrawUI();
		break;
	default:
	  break;
	}
}

void UI::DrawMenu()
{
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
	else if (displayMode == Circular)
		currentMenu = &CircMenu;
	else if (displayMode == Fourier || displayMode == Waterfall)
		currentMenu = &FftMenu;

	uint8_t pos = 0;
	for (auto m = currentMenu->cbegin(); m != currentMenu->cend(); m++, pos++) {
		lcd.DrawString(10, 32 + pos * 20, m->name, &lcd.Font_Large, (m->selected == EncoderModeL) ? LCD_BLACK : LCD_WHITE, (m->selected == EncoderModeL) ? LCD_WHITE : LCD_BLACK);
		lcd.DrawString(170, 32 + pos * 20, m->name, &lcd.Font_Large, (m->selected == EncoderModeR) ? LCD_BLACK : LCD_WHITE, (m->selected == EncoderModeR) ? LCD_WHITE : LCD_BLACK);
	}
}


void UI::handleEncoders()
{
	// encoders count in fours with the zero point set to 100
	if (std::abs((int16_t)32000 - (int16_t)L_ENC_CNT) > 3) {
		int8_t v = L_ENC_CNT > 32000 ? 1 : -1;
		if (menuMode)	MenuAction(&EncoderModeL, v);
		else			EncoderAction(EncoderModeL, v);

		L_ENC_CNT -= L_ENC_CNT > 32000 ? 4 : -4;
		cfg.ScheduleSave();
	}

	if (std::abs((int16_t)32000 - (int16_t)R_ENC_CNT) > 3) {
		int8_t v = R_ENC_CNT > 32000 ? 1 : -1;
		if (menuMode)	MenuAction(&EncoderModeR, v);
		else			EncoderAction(EncoderModeR, v);

		R_ENC_CNT -= R_ENC_CNT > 32000 ? 4 : -4;
		cfg.ScheduleSave();
	}

	if ((encoderBtnL || encoderBtnR) && menuMode) {
		encoderBtnL = encoderBtnR = menuMode = false;
		lcd.ScreenFill(LCD_BLACK);
		DrawUI();
		return;
	}

	// Menu mode
	if (encoderBtnR) {
		encoderBtnR = false;

		if (displayMode == Oscilloscope || displayMode == Circular || displayMode == Fourier) {
			menuMode = true;
			lcd.ScreenFill(LCD_BLACK);
			DrawMenu();
			return;
		}
	}

	// Change display mode (note recheck menuMode as interrupts can alter this mid-routine)
	if (encoderBtnL && !menuMode) {
		encoderBtnL = false;
		switch (displayMode) {
		case Oscilloscope :
			osc.sampleTimer = TIM3->ARR;
			displayMode = Fourier;
			break;
		case Fourier :		displayMode = Waterfall;		break;
		case Waterfall :	displayMode = Circular;			break;
		case Circular :		displayMode = MIDI;				break;
		case MIDI :
			TIM3->ARR = std::max(osc.sampleTimer, (uint16_t)MINSAMPLETIMER);
			displayMode = Oscilloscope;
			break;
		}
		cfg.ScheduleSave();
		ResetMode();
		return;
	}

}


void UI::ResetMode()
{
	TIM3->CR1 &= ~TIM_CR1_CEN;				// Disable the sample acquisiton timer
	UART4->CR1 &= ~USART_CR1_UE;			// Disable MIDI capture on UART4

	lcd.ScreenFill(LCD_BLACK);
	switch (displayMode) {
	case Oscilloscope :
		EncoderModeL = osc.encModeL;
		EncoderModeR = osc.encModeR;
		if (osc.sampleTimer > MINSAMPLETIMER)
			TIM3->ARR = osc.sampleTimer;
		break;
	case Fourier :
		EncoderModeL = fft.EncModeL;
		EncoderModeR = fft.EncModeR;
		break;
	case Waterfall :
		EncoderModeL = fft.wfallEncModeL;
		EncoderModeR = fft.wfallEncModeR;
		break;
	case Circular :
		EncoderModeL = osc.circEncModeL;
		EncoderModeR = osc.circEncModeR;
		break;
	case MIDI :
		break;
	}

	fft.capturing = osc.capturing = drawing = false;
	osc.bufferSamples = capturePos = oldAdc = 0;
	osc.circDrawing[0] = osc.circDrawing[1] = false;
	osc.setTriggerChannel();
	fft.dataAvailable[0] = fft.dataAvailable[1] = false;
	fft.samples = displayMode == Fourier ? fft.fftSamples : fft.WATERFALLSAMPLES;


	ui.DrawUI();

	if (displayMode == MIDI) {
		UART4->CR1 |= USART_CR1_UE;			// Enable MIDI capture
	} else {
		TIM3->CR1 |= TIM_CR1_CEN;			// Reenable the sample acquisiton timer
	}
}


std::string UI::EncoderLabel(encoderType type)
{
	switch (type) {
	case HorizScale :
		return "Zoom Horiz";
	case HorizScaleFine :
		return "Zoom Horiz";
	case ChannelSelect :
		return "Ch:" + std::string(osc.oscDisplay & 1 ? "A" : "") + std::string(osc.oscDisplay & 2 ? "B" : "") + std::string(osc.oscDisplay & 4 ? "C  " : "  ");
	case CalibVertScale :
		return "Calib Scale";
	case CalibVertOffset :
		return "Calib Offs";
	case VoltScale :
		return "Zoom Vert";
	case ZeroCross :
		return "Zero X: " + intToString(osc.CircZeroCrossings);
	case Trigger_X :
		return "Trigger X";
	case Trigger_Y :
		return "Trigger Y";
	case TriggerChannel :
		return std::string(osc.triggerChannel == channelA ? "Trigger A " : osc.triggerChannel == channelB ? "Trigger B " : osc.triggerChannel == channelC ? "Trigger C " : "No Trigger");
	case FFTAutoTune :
		return "Tune: " + std::string(fft.autoTune ? "auto" : "off ");
	case TraceOverlay :
		return "Trace: " + std::string(fft.traceOverlay ? "on " : "off ");
	case ActiveChannel :
		return "Channel " + std::string(fft.channel == channelA ? "A" : fft.channel == channelB ? "B" : "C");
	case MultiLane :
		return "Lanes: " + std::string(osc.multiLane ? "Yes" : "No ");
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
		ss << (int32_t)std::round(f * 10);
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

//	Takes an RGB colour and darkens by the specified amount
uint16_t UI::DarkenColour(const uint16_t& colour, uint16_t amount) {
	uint16_t r = (colour >> 11) << 1;
	uint16_t g = (colour >> 5) & 0b111111;
	uint16_t b = (colour & 0b11111) << 1;
	r -= std::min(amount, r);
	g -= std::min(amount, g);
	b -= std::min(amount, b);;
	return ((r >> 1) << 11) + (g << 5) + (b >> 1);
}
