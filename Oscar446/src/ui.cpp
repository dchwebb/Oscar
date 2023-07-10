#include "ui.h"
#include "lcd.h"
#include "fft.h"
#include "osc.h"
#include "tuner.h"
#include "config.h"

UI ui;


void UI::DrawUI()
{
	if (config.displayMode == DispMode::MIDI) {
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

	if (config.displayMode == DispMode::Oscilloscope) {
		std::string s = FloatToString(640000.0f * (TIM3->PSC + 1) * (TIM3->ARR + 1) / SystemCoreClock, false) + "ms    ";
		lcd.DrawString(140, lcd.drawHeight + 8, s, &lcd.Font_Small, LCD_WHITE, LCD_BLACK);
	}
}


void UI::MenuAction(encoderType* et, volatile const int8_t& val)
{
	const std::vector<MenuItem>* currentMenu =
			config.displayMode == DispMode::Tuner ? &tunerMenu :
			config.displayMode == DispMode::Oscilloscope ? &oscMenu :
			config.displayMode == DispMode::Fourier || config.displayMode == DispMode::Waterfall ? &fftMenu : nullptr;

	//	Move the selected menu item one forwards or one back based on value of encoder
	auto mi = std::find_if(currentMenu->cbegin(), currentMenu->cend(), [=] (MenuItem m) { return m.selected == *et; } );
	if ((mi != currentMenu->cbegin() && val < 0) || (mi != currentMenu->cend() - 1 && val > 0)) {
		mi += val;
		*et = mi->selected;
	}

	if (config.displayMode == DispMode::Oscilloscope) {
		osc.config.encModeL = encoderModeL;
		osc.config.encModeR = encoderModeR;
	} else if (config.displayMode == DispMode::Tuner) {
		tuner.config.encModeL = encoderModeL;
		tuner.config.encModeR = encoderModeR;
	} else if (config.displayMode == DispMode::Fourier) {
		fft.config.encModeL = encoderModeL;
		fft.config.encModeR = encoderModeR;
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
			if (config.displayMode == DispMode::Oscilloscope) {
				osc.config.sampleTimer = adj;
			}
			DrawUI();
		}
		break;
	case HorizScaleFine :
		TIM3->ARR += val;
		DrawUI();
		break;
	case CalibVertOffset :
		osc.config.vCalibOffset += 50 * val;
		break;
	case CalibVertScale :
		osc.config.vCalibScale += val * .01;
		break;
	case VoltScale :
		osc.config.voltScale -= val;
		osc.config.voltScale = std::clamp(static_cast<int>(osc.config.voltScale), 1, 12);
		break;
	case ChannelSelect :
		osc.config.oscDisplay += val;
		osc.config.oscDisplay = osc.config.oscDisplay == 0 ? 7 : osc.config.oscDisplay == 8 ? 1 : osc.config.oscDisplay;
		osc.setTriggerChannel();

		DrawUI();
		break;

	case TriggerChannel :
		if ((osc.config.triggerChannel == channelNone && val > 0) || (osc.config.triggerChannel == channelB && val < 0))
			osc.config.triggerChannel = channelA;
		else if ((osc.config.triggerChannel == channelA && val > 0) || (osc.config.triggerChannel == channelC && val < 0))
			osc.config.triggerChannel = channelB;
		else if ((osc.config.triggerChannel == channelB && val > 0) || (osc.config.triggerChannel == channelNone && val < 0))
			osc.config.triggerChannel = channelC;
		else if ((osc.config.triggerChannel == channelC && val > 0) || (osc.config.triggerChannel == channelA && val < 0))
			osc.config.triggerChannel = channelNone;

		osc.setTriggerChannel();

		DrawUI();
		break;
	case Trigger_Y :
		osc.config.triggerY = std::min(std::max((int32_t)osc.config.triggerY + 100 * val, (int32_t)3800), (int32_t)16000);
		break;
	case Trigger_X :
		osc.config.triggerX = std::min(std::max(osc.config.triggerX + 2 * val, 0), 316);
		break;
	case FFTAutoTune :
		fft.config.autoTune = !fft.config.autoTune;
		DrawUI();
		break;
	case TraceOverlay :
		if (config.displayMode == DispMode::Fourier) {
			fft.config.traceOverlay = !fft.config.traceOverlay;
		} else {
			tuner.config.traceOverlay = !tuner.config.traceOverlay;
			if (!tuner.config.traceOverlay) {
				tuner.ClearOverlay();
			}
		}
		DrawUI();
		break;
	case ActiveChannel :
		if (val > 0)
			fft.config.channel = (fft.config.channel == channelA) ? channelB : (fft.config.channel == channelB) ? channelC : channelA;
		else
			fft.config.channel = (fft.config.channel == channelA) ? channelC : (fft.config.channel == channelB) ? channelA : channelB;
		DrawUI();
		break;
	case MultiLane :
		osc.config.multiLane = !osc.config.multiLane;
		DrawUI();
		break;
	case TunerMode:
		tuner.config.mode = tuner.config.mode == Tuner::ZeroCrossing ? Tuner::FFT : Tuner::ZeroCrossing;
		tuner.Activate(true);
		DrawUI();
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
			config.displayMode == DispMode::Oscilloscope ? &oscMenu :
			config.displayMode == DispMode::Tuner ? &tunerMenu :
			config.displayMode == DispMode::Fourier || config.displayMode == DispMode::Waterfall ? &fftMenu : nullptr;

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
	if (GPIOA->IDR & GPIO_IDR_IDR_10 && leftBtnReleased == 0) {
		leftBtnReleased = SysTickVal;
	}
	if ((GPIOA->IDR & GPIO_IDR_IDR_10) == 0) {
		if (leftBtnReleased > 0 && leftBtnReleased < SysTickVal - 100) {
			encoderBtnL = true;
		}
		leftBtnReleased = 0;
	}
	if (GPIOB->IDR & GPIO_IDR_IDR_13 && rightBtnReleased == 0) {
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

		if (config.displayMode == DispMode::Oscilloscope || config.displayMode == DispMode::Tuner || config.displayMode == DispMode::Fourier) {
			menuMode = true;
			lcd.ScreenFill(LCD_BLACK);
			DrawMenu();
			return;
		}
	}

	// Change display mode (note recheck menuMode as interrupts can alter this mid-routine)
	if (encoderBtnL && !menuMode) {
		encoderBtnL = false;

		if (config.displayMode == DispMode::Oscilloscope) {
			osc.config.sampleTimer = TIM3->ARR;
		}

		switch (config.displayMode) {
			case DispMode::Oscilloscope:	config.displayMode = DispMode::Tuner;			break;
			case DispMode::Tuner:			config.displayMode = DispMode::Fourier;			break;
			case DispMode::Fourier:			config.displayMode = DispMode::Waterfall;		break;
			case DispMode::Waterfall:		config.displayMode = DispMode::MIDI;			break;
			case DispMode::MIDI:			config.displayMode = DispMode::Oscilloscope;	break;
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
	switch (config.displayMode) {
	case DispMode::Oscilloscope :
		encoderModeL = osc.config.encModeL;
		encoderModeR = osc.config.encModeR;
		TIM3->ARR = std::max(osc.config.sampleTimer, (uint16_t)MINSAMPLETIMER);
		break;
	case DispMode::Tuner :
		tuner.Activate(false);
		encoderModeL = tuner.config.encModeL;
		encoderModeR = tuner.config.encModeR;
		break;
	case DispMode::Fourier:
		fft.Activate();
		encoderModeL = fft.config.encModeL;
		encoderModeR = fft.config.encModeR;
		break;
	case DispMode::Waterfall :
		fft.Activate();
		encoderModeL = fft.wfallEncModeL;
		encoderModeR = fft.wfallEncModeR;
		break;
	case DispMode::MIDI :
		break;
	}

	osc.capturing = osc.drawing = false;
	osc.bufferSamples = osc.capturePos = osc.oldAdc = 0;
	osc.setTriggerChannel();

	ui.DrawUI();

	if (config.displayMode == DispMode::MIDI) {
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
		return "Ch:" + std::string(osc.config.oscDisplay & 1 ? "A" : "") + std::string(osc.config.oscDisplay & 2 ? "B" : "") + std::string(osc.config.oscDisplay & 4 ? "C  " : "  ");
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
		return std::string(osc.config.triggerChannel == channelA ? "Trigger A " : osc.config.triggerChannel == channelB ? "Trigger B " : osc.config.triggerChannel == channelC ? "Trigger C " : "No Trigger");
	case FFTAutoTune :
		return "Tune: " + std::string(fft.config.autoTune ? "auto" : "off ");
	case TraceOverlay :
		return "Trace: " + std::string((config.displayMode == DispMode::Fourier ? fft.config.traceOverlay : tuner.config.traceOverlay) ? "on " : "off ");
	case ActiveChannel :
		return "Channel " + std::string(fft.config.channel == channelA ? "A" : fft.config.channel == channelB ? "B" : "C");
	case MultiLane :
		return "Lanes: " + std::string(osc.config.multiLane ? "Yes" : "No ");
	case TunerMode :
		return tuner.config.mode == Tuner::ZeroCrossing ? "Zero Cross" : "FFT       ";
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


std::string UI::IntToString(const int32_t v)
{
	sprintf(charBuff, "%ld", v);
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


uint32_t UI::SerialiseConfig(uint8_t** buff)
{
	*buff = reinterpret_cast<uint8_t*>(&config);
	return sizeof(config);
}


uint32_t UI::StoreConfig(uint8_t* buff)
{
	if (buff != nullptr) {
		memcpy(&config, buff, sizeof(config));
	}
	return sizeof(config);
}
