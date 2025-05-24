#include "osc.h"
#include "ui.h"


void Osc::Capture()
{
	// Average the last four ADC readings to smooth noise
	adcA = adc.ChA_1 + adc.ChA_2 + adc.ChA_3 + adc.ChA_4;
	adcB = adc.ChB_1 + adc.ChB_2 + adc.ChB_3 + adc.ChB_4;
	adcC = adc.ChC_1 + adc.ChC_2 + adc.ChC_3 + adc.ChC_4;

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

	if (capturing && capturedSamples[captureBufferNumber] >= lcd.drawWidth) {
		volatile int susp [[maybe_unused]] = 1;
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

			// if trigger point not activating generate a temporary draw buffer
			if (bufferSamples > 1000 && capturePos == 0) {
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
		drawnVoltage = false;

		//	If in multi-lane mode get lane count from number of displayed channels and calculate vertical offset of channels B and C
		laneCount = (cfg.multiLane && cfg.oscDisplay == 0b111 ? 3 : cfg.multiLane && cfg.oscDisplay > 2 && cfg.oscDisplay != 4 ? 2 : 1);
		calculatedOffsetYB = (laneCount > 1 && cfg.oscDisplay & 0b001 ? lcd.drawHeight / laneCount : 0);
		calculatedOffsetYC = (laneCount == 2 ? lcd.drawHeight / 2 : laneCount == 3 ? lcd.drawHeight * 2 / 3 : 0);
	}



	// Check if drawing and that the sample capture is at or ahead of the draw position
	if (drawing && (oscBufferNumber != captureBufferNumber || capturedSamples[captureBufferNumber] >= drawPos || noTriggerDraw)) {
		// Calculate how many vertical strips can be drawn
		volatile uint32_t colsToDraw = 1;
		if (drawPos < textOffsetLeft) {
			colsToDraw = std::min(capturedSamples[oscBufferNumber], (uint16_t)textOffsetLeft) - drawPos;
		} else if (drawPos > textOffsetRight) {
			colsToDraw = std::min(capturedSamples[oscBufferNumber] - drawPos, lcd.drawWidth - drawPos);
		} else {
			colsToDraw = std::min((uint16_t)(std::min(capturedSamples[oscBufferNumber], (uint16_t)(textOffsetRight + 1)) - drawPos), lcd.drawBufferWidth);


		}

		if (colsToDraw > 106 || colsToDraw == 0) {
			volatile int susp [[maybe_unused]] = 1;
			return;
		}

		const uint8_t vOffset = (drawPos < textOffsetLeft || drawPos > textOffsetRight) ? textOffsetTop : 0;		// offset draw area so as not to overwrite voltage and freq labels
		const uint8_t drawHeight = lcd.drawHeight - (drawPos < textOffsetLeft ? textOffsetTop + 1 : 0);

		for (uint32_t col = 0; col < colsToDraw; ++col) {

			int32_t pos = col;		// draw buffer order is left to right, then down

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

			const uint16_t TY = CalcVertOffset(cfg.triggerY) + (triggerTest == &adcB ? calculatedOffsetYB : triggerTest == &adcC ? calculatedOffsetYC : 0);


			for (uint8_t h = vOffset; h <= drawHeight; ++h) {

				if ((drawPos > cfg.triggerX - 4 && drawPos < cfg.triggerX + 4 && TY == h) ||
				   (drawPos == cfg.triggerX && TY > h - 4 && TY < h + 4)) {
					lcd.drawBuffer[drawBufferNumber][pos] =  RGBColour::Yellow;			// Draw trigger
				} else if (cfg.oscDisplay & 1 && h >= AY.first && h <= AY.second) {
					lcd.drawBuffer[drawBufferNumber][pos] = RGBColour::Green;
				} else if (cfg.oscDisplay & 2 && h >= BY.first && h <= BY.second) {
					lcd.drawBuffer[drawBufferNumber][pos] = RGBColour::LightBlue;
				} else if (cfg.oscDisplay & 4 && h >= CY.first && h <= CY.second) {
					lcd.drawBuffer[drawBufferNumber][pos] = RGBColour::Orange;
				} else if (drawPos % 4 == 0 && (h + (lcd.drawHeight / (laneCount * 2))) % (lcd.drawHeight / (laneCount)) == 0) {						// 0v center mark
					lcd.drawBuffer[drawBufferNumber][pos] = RGBColour::Grey;
				} else {
					//lcd.drawBuffer[drawBufferNumber][pos] = (drawBufferNumber == 0 ? RGBColour::Black : RGBColour::Magenta);
					lcd.drawBuffer[drawBufferNumber][pos] = RGBColour::Black;
				}
				pos += colsToDraw;
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

			prevPixel = currentPos;			// Store previous sample so next sample can be drawn as a line from old to new
			++drawPos;
		}

		lcd.PatternFill(drawPos - colsToDraw, vOffset, drawPos - 1, drawHeight, lcd.drawBuffer[drawBufferNumber]);
		drawBufferNumber = drawBufferNumber == 0 ? 1 : 0;


		if (drawPos >= textOffsetLeft && !drawnVoltage && (oldVoltScale != cfg.voltScale || uiRefresh)) {// Write voltage
			drawnVoltage = true;
			lcd.DrawString(0, 1, " " + ui.IntToString(cfg.voltScale) + "v ", &lcd.Font_Small, RGBColour::Grey, RGBColour::Black);
			lcd.DrawString(0, lcd.drawHeight - 10, "-" + ui.IntToString(cfg.voltScale) + "v ", &lcd.Font_Small, RGBColour::Grey, RGBColour::Black);
		}

		if (drawPos >= lcd.drawWidth - 1){
			drawing = false;
			noTriggerDraw = false;
			debugPin.SetLow();

			// Write frequency
			if (noTriggerDraw) {
				lcd.DrawString(textOffsetRight, 1, "No Trigger " , &lcd.Font_Small, RGBColour::White, RGBColour::Black);
			} else {
				lcd.DrawString(textOffsetRight, 1, freq != 0 ? ui.FloatToString(freq, false) + "Hz    " : "          ", &lcd.Font_Small, RGBColour::White, RGBColour::Black);
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

	if (!freqBelowZero && freqSmoothY < calibZeroPos) {		// first time reading goes below zero
		freqBelowZero = true;
	}
	if (freqBelowZero && freqSmoothY >= calibZeroPos) {		// zero crossing
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



