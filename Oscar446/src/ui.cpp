#include "ui.h"

void UI::DrawUI()
{
	if (displayMode == DispMode::MIDI) {
		lcd.DrawString(120, lcd.drawHeight + 8, "MIDI Events", &lcd.Font_Small, LCD_GREY, LCD_BLACK);
		return;
	}

	// Draw UI
	lcd.DrawRect(0, lcd.drawHeight + 1, 319, 239, LCD_GREY);
	lcd.ColourFill(90, lcd.drawHeight + 1, 90, 239, LCD_GREY);
	lcd.ColourFill(230, lcd.drawHeight + 1, 230, 239, LCD_GREY);

	lcd.ColourFill(91, lcd.drawHeight + 2, 229, 238, LCD_BLACK);

	lcd.DrawString(10, lcd.drawHeight + 8, EncoderLabel(encoderModeL), &lcd.Font_Small, LCD_GREY, LCD_BLACK);
	lcd.DrawString(240, lcd.drawHeight + 8, EncoderLabel(encoderModeR), &lcd.Font_Small, LCD_GREY, LCD_BLACK);

	if (displayMode == DispMode::Oscilloscope) {
		std::string s = FloatToString(640000.0f * (TIM3->PSC + 1) * (TIM3->ARR + 1) / SystemCoreClock, false) + "ms    ";
		lcd.DrawString(140, lcd.drawHeight + 8, s, &lcd.Font_Small, LCD_WHITE, LCD_BLACK);
	}
}


void UI::MenuAction(encoderType* et, volatile const int8_t& val)
{
	const std::vector<MenuItem>* currentMenu =
			displayMode == DispMode::Tuner ? &tunerMenu :
			displayMode == DispMode::Oscilloscope ? &oscMenu :
			displayMode == DispMode::Fourier || displayMode == DispMode::Waterfall ? &fftMenu : nullptr;

	//	Move the selected menu item one forwards or one back based on value of encoder
	auto mi = std::find_if(currentMenu->cbegin(), currentMenu->cend(), [=] (MenuItem m) { return m.selected == *et; } );
	if ((mi != currentMenu->cbegin() && val < 0) || (mi != currentMenu->cend() - 1 && val > 0)) {
		mi += val;
		*et = mi->selected;
	}

	if (displayMode == DispMode::Oscilloscope) {
		osc.encModeL = encoderModeL;
		osc.encModeR = encoderModeR;
	} else if (displayMode == DispMode::Tuner) {
			tuner.encModeL = encoderModeL;
			tuner.encModeR = encoderModeR;
	} else if (displayMode == DispMode::Fourier) {
		fft.encModeL = encoderModeL;
		fft.encModeR = encoderModeR;
	}

	DrawMenu();
}


void UI::EncoderAction(encoderType type, const int8_t& val)
{
	int16_t adj;
	switch (type) {
	case HorizScale :
		adj = TIM3->ARR + (TIM3->ARR < 5000 ? 200 : TIM3->ARR < 20000 ? 400 : TIM3->ARR < 50000 ? 4000 : 8000) * -val;
		if (adj > MINSAMPLETIMER && adj < 560000) {
			TIM3->ARR = adj;
			if (displayMode == DispMode::Oscilloscope)		osc.sampleTimer = adj;
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
	case VoltScale :
		osc.voltScale -= val;
		osc.voltScale = std::clamp(static_cast<int>(osc.voltScale), 1, 12);
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

	const std::vector<MenuItem>* currentMenu =
			displayMode == DispMode::Oscilloscope? &oscMenu :
			displayMode == DispMode::Oscilloscope? &tunerMenu :
			displayMode == DispMode::Fourier || displayMode == DispMode::Waterfall ? &fftMenu : nullptr;

	uint8_t pos = 0;
	for (auto m = currentMenu->cbegin(); m != currentMenu->cend(); m++, pos++) {
		lcd.DrawString(10, 32 + pos * 20, m->name, &lcd.Font_Large, (m->selected == encoderModeL) ? LCD_BLACK : LCD_WHITE, (m->selected == encoderModeL) ? LCD_WHITE : LCD_BLACK);
		lcd.DrawString(170, 32 + pos * 20, m->name, &lcd.Font_Large, (m->selected == encoderModeR) ? LCD_BLACK : LCD_WHITE, (m->selected == encoderModeR) ? LCD_WHITE : LCD_BLACK);
	}
}


void UI::handleEncoders()
{
	// encoders count in fours with the zero point set to 100
	if (std::abs((int16_t)32000 - (int16_t)TIM4->CNT) > 3) {
		int8_t v = TIM4->CNT > 32000 ? 1 : -1;
		if (menuMode)	MenuAction(&encoderModeL, v);
		else			EncoderAction(encoderModeL, v);

		TIM4->CNT -= TIM4->CNT > 32000 ? 4 : -4;
		cfg.ScheduleSave();
	}

	if (std::abs((int16_t)32000 - (int16_t)TIM8->CNT) > 3) {
		int8_t v = TIM8->CNT > 32000 ? 1 : -1;
		if (menuMode)	MenuAction(&encoderModeR, v);
		else			EncoderAction(encoderModeR, v);

		TIM8->CNT -= TIM8->CNT > 32000 ? 4 : -4;
		cfg.ScheduleSave();
	}



	// Check if encoder buttons are pressed with debounce (L: PA10; R: PB13) 0 = pressed
	if (GPIOA->IDR & GPIO_IDR_IDR_10 && leftBtnReleased == 0) {		// button released
		leftBtnReleased = SysTickVal;
	}
	if ((GPIOA->IDR & GPIO_IDR_IDR_10) == 0) {
		if (leftBtnReleased > 0 && leftBtnReleased < SysTickVal - 100) {
			encoderBtnL = true;
		}
		leftBtnReleased = 0;
	}
	if (GPIOB->IDR & GPIO_IDR_IDR_13 && rightBtnReleased == 0) {		// button released
		rightBtnReleased = SysTickVal;
	}
	if ((GPIOB->IDR & GPIO_IDR_IDR_13) == 0) {
		if (rightBtnReleased > 0 && rightBtnReleased < SysTickVal - 100) {
			encoderBtnR = true;
		}
		rightBtnReleased = 0;
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

		if (displayMode == DispMode::Oscilloscope || displayMode == DispMode::Tuner || displayMode == DispMode::Fourier) {
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
		case DispMode::Oscilloscope :
			osc.sampleTimer = TIM3->ARR;
			displayMode = DispMode::Tuner;
			break;
		case DispMode::Tuner:			displayMode = DispMode::Fourier;	break;
		case DispMode::Fourier :		displayMode = DispMode::Waterfall;	break;
		case DispMode::Waterfall :		displayMode = DispMode::MIDI;		break;
		case DispMode::MIDI :
			TIM3->ARR = std::max(osc.sampleTimer, (uint16_t)MINSAMPLETIMER);
			displayMode = DispMode::Oscilloscope;
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
	case DispMode::Oscilloscope :
		encoderModeL = osc.encModeL;
		encoderModeR = osc.encModeR;
		if (osc.sampleTimer > MINSAMPLETIMER)
			TIM3->ARR = osc.sampleTimer;
		break;
	case DispMode::Tuner :
		encoderModeL = tuner.encModeL;
		encoderModeR = tuner.encModeR;
		break;
	case DispMode::Fourier :
		encoderModeL = fft.encModeL;
		encoderModeR = fft.encModeR;
		break;
	case DispMode::Waterfall :
		encoderModeL = fft.wfallEncModeL;
		encoderModeR = fft.wfallEncModeR;
		break;
	case DispMode::MIDI :
		break;
	}

	fft.capturing = osc.capturing = osc.drawing = false;
	osc.bufferSamples = osc.capturePos = osc.oldAdc = 0;
	osc.setTriggerChannel();
	fft.dataAvailable[0] = fft.dataAvailable[1] = false;
	fft.samples = displayMode == DispMode::Fourier ? fft.fftSamples : fft.waterfallSamples;


	ui.DrawUI();

	if (displayMode == DispMode::MIDI) {
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
	case ZeroCrossing :
		return "Zero Cross";
	default:
	  return "";
	}
}


std::string UI::FloatToString(float f, bool smartFormat)
{
	if (smartFormat && f > 10000) {
		sprintf(charBuff, "%.1fk", std::round(f / 100.0f) / 10.0f);
	} else if (smartFormat && f > 1000) {
		sprintf(charBuff, "%.0f", std::round(f));
	} else	{
		sprintf(charBuff, "%.1f", std::round(f * 10.0f) / 10.0f);
	}
	return std::string(charBuff);
}


std::string UI::IntToString(const uint16_t v)
{
	sprintf(charBuff, "%u", v);
	return std::string(charBuff);
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
