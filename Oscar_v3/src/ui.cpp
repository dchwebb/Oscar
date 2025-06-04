#include "ui.h"
#include "lcd.h"
#include "fft.h"
#include "osc.h"
#include "tuner.h"


void UI::DrawUI()
{
	if (cfg.displayMode == DispMode::MIDI) {
		lcd.DrawString(120, lcd.drawHeight + 8, "MIDI Events", lcd.Font_Small, RGBColour::Grey, RGBColour::Black);
		return;
	}

	// Draw UI
	lcd.DrawRect(0, lcd.drawHeight + 1, 319, 239, RGBColour::Grey);
	lcd.ColourFill(90, lcd.drawHeight + 1, 90, 239, RGBColour::Grey);
	lcd.ColourFill(230, lcd.drawHeight + 1, 230, 239, RGBColour::Grey);

	lcd.ColourFill(91, lcd.drawHeight + 2, 229, 238, RGBColour::Black);

	lcd.DrawString(10, lcd.drawHeight + 8, EncoderLabel(encoderModeL), lcd.Font_Small, RGBColour::Grey, RGBColour::Black);
	lcd.DrawString(240, lcd.drawHeight + 8, EncoderLabel(encoderModeR), lcd.Font_Small, RGBColour::Grey, RGBColour::Black);

	if (cfg.displayMode == DispMode::Oscilloscope) {
		std::string s = FloatToString(640000.0f * (TIM3->ARR + 1) / SystemCoreClock, false) + "ms    ";
		lcd.DrawString(140, lcd.drawHeight + 8, s, lcd.Font_Small, RGBColour::White, RGBColour::Black);
		osc.uiRefresh = true;
	}

	// Channel button leds
	const bool fftMode = (cfg.displayMode == DispMode::Fourier || cfg.displayMode == DispMode::Tuner || cfg.displayMode == DispMode::Waterfall);
	channelSelect.ledChA.Set(
			(osc.cfg.oscDisplay & 1 && cfg.displayMode == DispMode::Oscilloscope) ||
			(fft.cfg.channel == channelA && fftMode));
	channelSelect.ledChB.Set(
			(osc.cfg.oscDisplay & 2 && cfg.displayMode == DispMode::Oscilloscope) ||
			(fft.cfg.channel == channelB && fftMode));
	channelSelect.ledChC.Set(
			(osc.cfg.oscDisplay & 4 && cfg.displayMode == DispMode::Oscilloscope) ||
			(fft.cfg.channel == channelC && fftMode));

}


void UI::MenuAction(encoderType* et, volatile const int8_t& val)
{
	const std::vector<MenuItem>* currentMenu =
			cfg.displayMode == DispMode::Tuner ? &tunerMenu :
			cfg.displayMode == DispMode::Oscilloscope ? &oscMenu :
			cfg.displayMode == DispMode::Fourier || cfg.displayMode == DispMode::Waterfall ? &fftMenu : nullptr;

	//	Move the selected menu item one forwards or one back based on value of encoder
	auto mi = std::find_if(currentMenu->cbegin(), currentMenu->cend(), [=] (MenuItem m) { return m.selected == *et; } );
	if ((mi != currentMenu->cbegin() && val < 0) || (mi != currentMenu->cend() - 1 && val > 0)) {
		mi += val;
		*et = mi->selected;
	}

	if (cfg.displayMode == DispMode::Oscilloscope) {
		osc.cfg.encModeL = encoderModeL;
		osc.cfg.encModeR = encoderModeR;
	} else if (cfg.displayMode == DispMode::Tuner) {
		tuner.cfg.encModeL = encoderModeL;
		tuner.cfg.encModeR = encoderModeR;
	} else if (cfg.displayMode == DispMode::Fourier) {
		fft.cfg.encModeL = encoderModeL;
		fft.cfg.encModeR = encoderModeR;
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
			SetSampleTimer(adj);
			if (cfg.displayMode == DispMode::Oscilloscope) {
				osc.cfg.sampleTimer = adj;
			}
			DrawUI();
		}
		break;
	case HorizScaleFine :
		SetSampleTimer(TIM3->ARR + val);
		DrawUI();
		break;
	case CalibVertOffset :
		osc.cfg.vCalibOffset += 50 * val;
		break;
	case CalibVertScale :
		osc.cfg.vCalibScale += val * .01;
		break;
	case VoltScale :
		osc.cfg.voltScale -= val;
		osc.cfg.voltScale = std::clamp(static_cast<int>(osc.cfg.voltScale), 1, 12);
		break;

	case TriggerChannel :
		if ((osc.cfg.triggerChannel == channelNone && val > 0) || (osc.cfg.triggerChannel == channelB && val < 0))
			osc.cfg.triggerChannel = channelA;
		else if ((osc.cfg.triggerChannel == channelA && val > 0) || (osc.cfg.triggerChannel == channelC && val < 0))
			osc.cfg.triggerChannel = channelB;
		else if ((osc.cfg.triggerChannel == channelB && val > 0) || (osc.cfg.triggerChannel == channelNone && val < 0))
			osc.cfg.triggerChannel = channelC;
		else if ((osc.cfg.triggerChannel == channelC && val > 0) || (osc.cfg.triggerChannel == channelA && val < 0))
			osc.cfg.triggerChannel = channelNone;

		osc.setTriggerChannel();

		DrawUI();
		break;
	case Trigger_Y :
		osc.cfg.triggerY = std::min(std::max((int32_t)osc.cfg.triggerY + 100 * val, (int32_t)3800), (int32_t)16000);
		break;
	case Trigger_X :
		osc.cfg.triggerX = std::min(std::max(osc.cfg.triggerX + 2 * val, 0), 316);
		break;
	case FFTAutoTune :
		fft.cfg.autoTune = !fft.cfg.autoTune;
		DrawUI();
		break;
	case TraceOverlay :
		if (cfg.displayMode == DispMode::Fourier) {
			fft.cfg.traceOverlay = !fft.cfg.traceOverlay;
		} else {
			tuner.cfg.traceOverlay = !tuner.cfg.traceOverlay;
			if (!tuner.cfg.traceOverlay) {
				tuner.ClearOverlay();
			}
		}
		DrawUI();
		break;
	case MultiLane :
		osc.cfg.multiLane = !osc.cfg.multiLane;
		DrawUI();
		break;
	case TunerMode:
		tuner.cfg.mode = tuner.cfg.mode == Tuner::ZeroCrossing ? Tuner::FFT : Tuner::ZeroCrossing;
		tuner.Activate(true);
		DrawUI();
	default:
	  break;
	}
}


void UI::DrawMenu()
{
	lcd.DrawString(10, 6, "L", lcd.Font_Large, RGBColour::White, RGBColour::Black);
	lcd.DrawString(80, 6, "Encoder Action", lcd.Font_Large, RGBColour::Orange, RGBColour::Black);
	lcd.DrawString(303, 6, "R", lcd.Font_Large, RGBColour::White, RGBColour::Black);
	lcd.DrawRect(0, 1, 319, 239, RGBColour::White);
	lcd.DrawLine(0, 27, 319, 27, RGBColour::White);
	lcd.DrawLine(26, 1, 26, 27, RGBColour::White);
	lcd.DrawLine(294, 1, 294, 27, RGBColour::White);
	lcd.DrawLine(159, 27, 159, 239, RGBColour::White);

	const std::vector<MenuItem>* currentMenu =
			cfg.displayMode == DispMode::Oscilloscope ? &oscMenu :
			cfg.displayMode == DispMode::Tuner ? &tunerMenu :
			cfg.displayMode == DispMode::Fourier || cfg.displayMode == DispMode::Waterfall ? &fftMenu : nullptr;

	uint8_t pos = 0;
	for (auto m = currentMenu->cbegin(); m != currentMenu->cend(); m++, pos++) {
		lcd.DrawString(10, 32 + pos * 20, m->name, lcd.Font_Large, (m->selected == encoderModeL) ? RGBColour::Black : RGBColour::White, (m->selected == encoderModeL) ? RGBColour::White : RGBColour::Black);
		lcd.DrawString(170, 32 + pos * 20, m->name, lcd.Font_Large, (m->selected == encoderModeR) ? RGBColour::Black : RGBColour::White, (m->selected == encoderModeR) ? RGBColour::White : RGBColour::Black);
	}
}


void UI::handleEncoders()
{
	// encoders count in fours with the zero point set to 100
	if (std::abs((int16_t)32000 - (int16_t)TIM4->CNT) > 3) {
#ifdef REVERSEENCODERS
		int8_t v = TIM4->CNT > 32000 ? -1 : 1;
#else
		int8_t v = TIM4->CNT > 32000 ? 1 : -1;
#endif
		if (menuMode)	MenuAction(&encoderModeR, v);
		else			EncoderAction(encoderModeR, v);

		TIM4->CNT -= TIM4->CNT > 32000 ? 4 : -4;
		config.ScheduleSave();
	}

	if (std::abs((int16_t)32000 - (int16_t)TIM2->CNT) > 3) {
#ifdef REVERSEENCODERS
		int8_t v = TIM2->CNT > 32000 ? -1 : 1;
#else
		int8_t v = TIM2->CNT > 32000 ? 1 : -1;
#endif
		if (menuMode)	MenuAction(&encoderModeL, v);
		else			EncoderAction(encoderModeL, v);

		TIM2->CNT -= TIM2->CNT > 32000 ? 4 : -4;
		config.ScheduleSave();
	}

	bool encoderBtnL = btnEncL.Pressed();
	bool encoderBtnR = btnEncR.Pressed();
	bool menuButton = btnMenu.Pressed();

	if (menuMode && (encoderBtnL || encoderBtnR || menuButton)) {
		menuMode = false;
		lcd.ScreenFill(RGBColour::Black);
		DrawUI();
		return;
	}

	if (menuButton && (cfg.displayMode == DispMode::Oscilloscope || cfg.displayMode == DispMode::Tuner || cfg.displayMode == DispMode::Fourier)) {
		menuMode = true;
		lcd.ScreenFill(RGBColour::Black);
		DrawMenu();
		return;
	}

	// Change display mode
	if (encoderBtnL || encoderBtnR) {
		if (cfg.displayMode == DispMode::Oscilloscope) {
			osc.cfg.sampleTimer = TIM3->ARR;
		}
		if (encoderBtnL) {
			switch (cfg.displayMode) {
				case DispMode::Oscilloscope:	cfg.displayMode = DispMode::MIDI;			break;
				case DispMode::Tuner:			cfg.displayMode = DispMode::Oscilloscope;	break;
				case DispMode::Fourier:			cfg.displayMode = DispMode::Tuner;			break;
				case DispMode::Waterfall:		cfg.displayMode = DispMode::Fourier;		break;
				case DispMode::MIDI:			cfg.displayMode = DispMode::Waterfall;		break;
			}
		} else {
			switch (cfg.displayMode) {
				case DispMode::Oscilloscope:	cfg.displayMode = DispMode::Tuner;			break;
				case DispMode::Tuner:			cfg.displayMode = DispMode::Fourier;		break;
				case DispMode::Fourier:			cfg.displayMode = DispMode::Waterfall;		break;
				case DispMode::Waterfall:		cfg.displayMode = DispMode::MIDI;			break;
				case DispMode::MIDI:			cfg.displayMode = DispMode::Oscilloscope;	break;
			}
		}
		config.ScheduleSave();
		ResetMode();
		return;
	}

	// Channel select buttons
	if (cfg.displayMode != DispMode::MIDI) {
		uint32_t btnPressed = channelSelect.btnChA.Pressed() |
							 (channelSelect.btnChB.Pressed() << 1) |
							 (channelSelect.btnChC.Pressed() << 2);		// Bit representation of pressed buttons
		for (uint32_t i = 0; i < 3; ++i) {
			uint32_t bit = (1 << i);
			if (btnPressed & bit) {
				if (cfg.displayMode == DispMode::Oscilloscope) {

					bool currentState = (osc.cfg.oscDisplay & bit);		// Is button currently pressed
					osc.cfg.oscDisplay &= (0b111 ^ bit);				// Clear display bit
					if (!currentState || osc.cfg.oscDisplay == 0) {
						osc.cfg.oscDisplay |= bit;						// Set display bit if currently off or no channel selected
						config.ScheduleSave();
					}
					osc.setTriggerChannel();
				} else {						// Tuner, FFT, waterfall
					fft.cfg.channel = (oscChannel)i;
				}
				DrawUI();
			}
		}

	}
}


void UI::ResetMode()
{
	RunSampleTimer(false);					// Disable the sample acquisiton timer
	UART4->CR1 &= ~USART_CR1_UE;			// Disable MIDI capture on UART4

	lcd.ScreenFill(RGBColour::Black);
	switch (cfg.displayMode) {
	case DispMode::Oscilloscope :
		encoderModeL = osc.cfg.encModeL;
		encoderModeR = osc.cfg.encModeR;
		SetSampleTimer(std::max(osc.cfg.sampleTimer, (uint16_t)MINSAMPLETIMER));
		break;
	case DispMode::Tuner :
		tuner.Activate(false);
		encoderModeL = tuner.cfg.encModeL;
		encoderModeR = tuner.cfg.encModeR;
		break;
	case DispMode::Fourier:
		fft.Activate();
		encoderModeL = fft.cfg.encModeL;
		encoderModeR = fft.cfg.encModeR;
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

	if (cfg.displayMode == DispMode::MIDI) {
		UART4->CR1 |= USART_CR1_UE;			// Enable MIDI capture
	} else {
		RunSampleTimer(true);				// Reenable the sample acquisiton timer
	}
}


std::string_view UI::EncoderLabel(encoderType type)
{
	switch (type) {
	case HorizScale :
		return "Zoom Horiz";
	case HorizScaleFine :
		return "Zoom Horiz";
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
		return std::string(osc.cfg.triggerChannel == channelA ? "Trigger A " : osc.cfg.triggerChannel == channelB ? "Trigger B " : osc.cfg.triggerChannel == channelC ? "Trigger C " : "No Trigger");
	case FFTAutoTune :
		return "Tune: " + std::string(fft.cfg.autoTune ? "auto" : "off ");
	case TraceOverlay :
		return "Trace: " + std::string((cfg.displayMode == DispMode::Fourier ? fft.cfg.traceOverlay : tuner.cfg.traceOverlay) ? "on " : "off ");
	case MultiLane :
		return "Lanes: " + std::string(osc.cfg.multiLane ? "Yes" : "No ");
	case TunerMode :
		return tuner.cfg.mode == Tuner::ZeroCrossing ? "Zero Cross" : "FFT       ";
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



