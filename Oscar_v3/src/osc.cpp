#include "osc.h"
#include "ui.h"


void Osc::Activate()
{

	osc.capturing = false;
	osc.drawing = false;
	osc.bufferSamples = 0;
	osc.capturePos = 0;
	osc.oldAdc = 0;
	freqLvlAv = calibZeroPos;
	osc.setTriggerChannel();
	SetSampleTimer(std::max(osc.cfg.sampleTimer, minSampleTimer));
}


void Osc::Capture()
{
	// Average the last four ADC readings to smooth noise
	adcA = adc.ChannelSummed(channelA);
	adcB = adc.ChannelSummed(channelB);
	adcC = adc.ChannelSummed(channelC);

	// check if we should start capturing - ie not drawing from the capture buffer and crossed over the trigger threshold (or in free mode)
	if (!capturing && (!drawing || captureBufferNumber != oscBufferNumber) &&
			(triggerTest == nullptr || (bufferSamples > cfg.triggerX && oldAdc < cfg.triggerY && *triggerTest >= cfg.triggerY))) {
		capturing = true;

		if (triggerTest == nullptr) {									// free running mode
			capturePos = 0;
			drawOffset[captureBufferNumber] = 0;
			capturedSamples[captureBufferNumber] = -1;
		} else {
			// calculate the drawing offset based on the current capture position minus the horizontal trigger position
			drawOffset[captureBufferNumber] = capturePos - cfg.triggerX;
			if (drawOffset[captureBufferNumber] < 0)
				drawOffset[captureBufferNumber] += lcd.drawWidth;

			capturedSamples[captureBufferNumber] = cfg.triggerX - 1;	// used to check if a sample is ready to be drawn
		}
	}

	// if capturing check if write buffer is full and switch to next buffer if so; next buffer will only be filled if not being drawn from
	if (capturing && capturedSamples[captureBufferNumber] == lcd.drawWidth - 1) {
		captureBufferNumber = captureBufferNumber == 1 ? 0 : 1;		// switch the capture buffer
		bufferSamples = 0;			// stores number of samples captured since switching buffers to ensure triggered mode works correctly
		capturing = false;
	}

	// If capturing or buffering samples waiting for trigger store current readings in buffer and increment counters
	if (capturing || !drawing || captureBufferNumber != oscBufferNumber) {

		OscBufferA[captureBufferNumber][capturePos] = adcA;
		OscBufferB[captureBufferNumber][capturePos] = adcB;
		OscBufferC[captureBufferNumber][capturePos] = adcC;
		oldAdc = *triggerTest;

		if (capturePos == lcd.drawWidth - 1) {
			capturePos = 0;
		} else {
			++capturePos;
		}

		capturedSamples[captureBufferNumber]++;
		if (!capturing) {
			++bufferSamples;

			// If trigger point not activating generate a temporary draw buffer
			// For slow sample rates, number of buffer samples needed for 'no trigger' draw should be reduced
			const uint32_t tiggerLevel = 100000000 / cfg.sampleTimer;
			if (bufferSamples > tiggerLevel && capturePos == 0) {
				captureBufferNumber = captureBufferNumber == 1 ? 0 : 1;		// switch the capture buffer
				bufferSamples = 0;
				drawOffset[captureBufferNumber] = 0;
				noTriggerDraw = true;
			}
		}
	}
}


// Main loop to display oscilloscope trace
void Osc::OscRun()
{
	// check if we should start drawing
	if (!drawing && (capturing || noTriggerDraw)) {
		debugPin.SetHigh();

		oscBufferNumber = noTriggerDraw ? !(bool)captureBufferNumber : captureBufferNumber;
		drawing = true;
		drawPos = 0;

		//	If in multi-lane mode get lane count from number of displayed channels and calculate vertical offset of channels B and C
		laneCount = (cfg.multiLane && cfg.oscDisplay == 0b111 ? 3 : cfg.multiLane && cfg.oscDisplay > 2 && cfg.oscDisplay != 4 ? 2 : 1);
		calculatedOffsetYB = (laneCount > 1 && cfg.oscDisplay & 0b001 ? lcd.drawHeight / laneCount : 0);
		calculatedOffsetYC = (laneCount == 2 ? lcd.drawHeight / 2 : laneCount == 3 ? lcd.drawHeight * 2 / 3 : 0);
	}

	// Check if drawing and that the sample capture is at or ahead of the draw position
	if (drawing && (oscBufferNumber != captureBufferNumber || capturedSamples[captureBufferNumber] >= drawPos || noTriggerDraw)) {
		// Calculate offset between capture and drawing positions to display correct sample
		const uint16_t offsetX = (drawOffset[oscBufferNumber] + drawPos) % lcd.drawWidth;

		SamplePos currentPos = VertOffsets(offsetX);

		// Starting a new screen: Set previous pixel to current pixel and clear frequency calculations
		if (drawPos == 0) {
			prevPixel = currentPos;
			freqBelowZero = false;
			freqCrossZero = 0;
		}

		FreqCalc(offsetX);

		// create draw buffer
		const std::pair<uint16_t, uint16_t> AY = std::minmax(currentPos.pos[Channel::A], prevPixel.pos[Channel::A]);
		const std::pair<uint16_t, uint16_t> BY = std::minmax(currentPos.pos[Channel::B], prevPixel.pos[Channel::B]);
		const std::pair<uint16_t, uint16_t> CY = std::minmax(currentPos.pos[Channel::C], prevPixel.pos[Channel::C]);

		const uint8_t vOffset = (drawPos < textOffsetLeft || drawPos > textOffsetRight) ? textOffsetTop : 0;		// offset draw area so as not to overwrite voltage and freq labels
		for (uint8_t h = vOffset; h <= lcd.drawHeight - (drawPos < textOffsetLeft ? textOffsetTop + 1 : 0); ++h) {

			if (cfg.oscDisplay & 1 && h >= AY.first && h <= AY.second) {
				lcd.drawBuffer[drawBufferNumber][h - vOffset] = RGBColour::Green;
			} else if (cfg.oscDisplay & 2 && h >= BY.first && h <= BY.second) {
				lcd.drawBuffer[drawBufferNumber][h - vOffset] = RGBColour::LightBlue;
			} else if (cfg.oscDisplay & 4 && h >= CY.first && h <= CY.second) {
				lcd.drawBuffer[drawBufferNumber][h - vOffset] = RGBColour::Orange;
			} else if (drawPos % 4 == 0 && (h + (lcd.drawHeight / (laneCount * 2))) % (lcd.drawHeight / (laneCount)) == 0) {						// 0v center mark
				lcd.drawBuffer[drawBufferNumber][h - vOffset] = RGBColour::Grey;
			} else {
				lcd.drawBuffer[drawBufferNumber][h - vOffset] = RGBColour::Black;
			}
		}

		// Draw grey lines indicating voltage range for one channel or channel divisions for 2 or 3 channels
		if (drawPos < 5) {
			for (int m = 1; m < (laneCount == 1 ? cfg.voltScale * 2 : (laneCount * 2)); ++m) {
				int vPos = m * lcd.drawHeight / (laneCount == 1 ? cfg.voltScale * 2 : (laneCount * 2)) - textOffsetTop;
				if (vPos > 0) {
					lcd.drawBuffer[drawBufferNumber][vPos] = RGBColour::Grey;
				}
			}
		}

		lcd.PatternFill(drawPos, vOffset, drawPos, lcd.drawHeight - (drawPos < textOffsetLeft ? textOffsetTop + 1 : 0), lcd.drawBuffer[drawBufferNumber]);
		drawBufferNumber = drawBufferNumber == 0 ? 1 : 0;

		prevPixel = currentPos;			// Store previous sample so next sample can be drawn as a line from old to new

		++drawPos;
		if (drawPos == lcd.drawWidth){
			drawing = false;
			noTriggerDraw = false;
			debugPin.SetLow();
		}

		// Draw trigger as a yellow cross
		if (drawPos == cfg.triggerX + 4) {
			const uint16_t vo = CalcVertOffset(cfg.triggerY) + (triggerTest == &adcB ? calculatedOffsetYB : triggerTest == &adcC ? calculatedOffsetYC : 0);
			if (vo > 4 && vo < lcd.drawHeight - 4) {
				lcd.DrawLine(cfg.triggerX, vo - 4, cfg.triggerX, vo + 4, RGBColour::Yellow);
				lcd.DrawLine(std::max(cfg.triggerX - 4, 0), vo, cfg.triggerX + 4, vo, RGBColour::Yellow);
			}
		}

		if (drawPos == 1) {
			// Write voltage
			if (oldVoltScale != cfg.voltScale || uiRefresh) {
				lcd.DrawString(0, 1, " " + ui.IntToString(cfg.voltScale) + "v ", lcd.Font_Small, RGBColour::Grey, RGBColour::Black);
				lcd.DrawString(0, lcd.drawHeight - 10, "-" + ui.IntToString(cfg.voltScale) + "v ", lcd.Font_Small, RGBColour::Grey, RGBColour::Black);
				oldVoltScale = cfg.voltScale;
				uiRefresh = false;
			}

			// Write frequency
			if (noTriggerDraw) {
				lcd.DrawString(textOffsetRight, 1, "No Trigger " , lcd.Font_Small, RGBColour::White, RGBColour::Black);
			} else {
				lcd.DrawString(textOffsetRight, 1, freq != 0 ? ui.FloatToString(freq, false) + "Hz     " : "          ", lcd.Font_Small, RGBColour::White, RGBColour::Black);
			}
		}

	}
}


Osc::SamplePos Osc::VertOffsets(uint16_t offsetX)
{
	return SamplePos {
		CalcVertOffset(OscBufferA[oscBufferNumber][offsetX]),
		(uint16_t)(CalcVertOffset(OscBufferB[oscBufferNumber][offsetX]) + calculatedOffsetYB),
		(uint16_t)(CalcVertOffset(OscBufferC[oscBufferNumber][offsetX]) + calculatedOffsetYC)

	};
}


uint16_t Osc::CalcVertOffset(const uint16_t& vPos)
{
	// Calculates vertical offset of oscilloscope trace from raw ADC value
	const float vOffset = (((static_cast<float>(vPos * cfg.vCalibScale + cfg.vCalibOffset) / (4.0f * 4096.0f) - 0.5f) * (8.0f / cfg.voltScale)) + 0.5f) / laneCount * lcd.drawHeight;
	return std::clamp(vOffset, 1.0f, static_cast<float>((lcd.drawHeight - 1) / laneCount));

}


float Osc::FreqFromPos(const uint16_t pos)
{
	// returns frequency of signal based on number of samples wide the signal is versus the sampling rate
	return samplingFrequency / pos;
}


void Osc::FreqCalc(const uint16_t offsetX) {
	//	frequency calculation - detect upwards zero crossings
	const uint16_t currentY = (cfg.oscDisplay & 1) ? OscBufferA[oscBufferNumber][offsetX] :
							  (cfg.oscDisplay & 2) ? OscBufferB[oscBufferNumber][offsetX] :
							   OscBufferC[oscBufferNumber][offsetX];
	freqSmoothY = (drawPos == 0) ? currentY : (currentY + 15 * freqSmoothY) / 16;
	freqLvlSum = (drawPos == 0) ? 0 : freqLvlSum + freqSmoothY;

	if (!freqBelowZero && freqSmoothY < freqLvlAv) {		// first time reading goes below zero
		freqBelowZero = true;
	}
	if (freqBelowZero && freqSmoothY >= freqLvlAv) {		// zero crossing
		//	second zero crossing - calculate frequency averaged over a number passes to smooth
		if (freqCrossZero > 0 && drawPos - freqCrossZero > 3) {
			const float newFreq = FreqFromPos(drawPos - freqCrossZero);
			if (std::abs(freq - newFreq) / freq < .05f) {
				freq = 0.8f * freq + 0.2f * newFreq;
			} else {
				freq = newFreq;
			}
		}
		freqCrossZero = drawPos;
		freqBelowZero = false;
	}

	if (drawPos == lcd.drawWidth - 1) {
		freqLvlAv = freqLvlSum / lcd.drawWidth;
	}
}

void Osc::setTriggerChannel()
{
	// Choose the trigger channel based on cfg preference and channel visibility
	triggerTest = cfg.triggerChannel == channelNone ? nullptr :
					 (cfg.triggerChannel == channelC && (cfg.oscDisplay & 4)) || cfg.oscDisplay == 4 ? &adcC :
					 (cfg.triggerChannel == channelB || !(cfg.oscDisplay & 1)) && (cfg.oscDisplay & 2) ? &adcB : &adcA;

}


void Osc::CalcZeroSize() {			// returns ADC size that corresponds to 0v
	calibZeroPos = (8192 - cfg.vCalibOffset) / cfg.vCalibScale;
}



