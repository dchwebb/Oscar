#include "osc.h"
#include "ui.h"

Osc osc;

void Osc::Capture()
{
	// Average the last four ADC readings to smooth noise
	adcA = ADC_array[0] + ADC_array[3] + ADC_array[6] + ADC_array[9];
	adcB = ADC_array[1] + ADC_array[4] + ADC_array[7] + ADC_array[10];
	adcC = ADC_array[2] + ADC_array[5] + ADC_array[8] + ADC_array[11];

	// check if we should start capturing - ie not drawing from the capture buffer and crossed over the trigger threshold (or in free mode)
	if (!capturing && (!drawing || captureBufferNumber != oscBufferNumber) &&
			(triggerTest == nullptr || (bufferSamples > config.triggerX && oldAdc < config.triggerY && *triggerTest >= config.triggerY))) {
		capturing = true;

		if (triggerTest == nullptr) {									// free running mode
			capturePos = 0;
			drawOffset[captureBufferNumber] = 0;
			capturedSamples[captureBufferNumber] = -1;
		} else {
			// calculate the drawing offset based on the current capture position minus the horizontal trigger position
			drawOffset[captureBufferNumber] = capturePos - config.triggerX;
			if (drawOffset[captureBufferNumber] < 0)
				drawOffset[captureBufferNumber] += lcd.drawWidth;

			capturedSamples[captureBufferNumber] = config.triggerX - 1;	// used to check if a sample is ready to be drawn
		}
	}

	// if capturing check if write buffer is full and switch to next buffer if so; if not full store current reading
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
		oscBufferNumber = noTriggerDraw ? !(bool)captureBufferNumber : captureBufferNumber;
		drawing = true;
		drawPos = 0;

		//	If in multi-lane mode get lane count from number of displayed channels and calculate vertical offset of channels B and C
		laneCount = (config.multiLane && config.oscDisplay == 0b111 ? 3 : config.multiLane && config.oscDisplay > 2 && config.oscDisplay != 4 ? 2 : 1);
		calculatedOffsetYB = (laneCount > 1 && config.oscDisplay & 0b001 ? lcd.drawHeight / laneCount : 0);
		calculatedOffsetYC = (laneCount == 2 ? lcd.drawHeight / 2 : laneCount == 3 ? lcd.drawHeight * 2 / 3 : 0);
	}

	// Check if drawing and that the sample capture is at or ahead of the draw position
	if (drawing && (oscBufferNumber != captureBufferNumber || capturedSamples[captureBufferNumber] >= drawPos || noTriggerDraw)) {
		// Calculate offset between capture and drawing positions to display correct sample
		const uint16_t calculatedOffsetX = (drawOffset[oscBufferNumber] + drawPos) % lcd.drawWidth;

		const uint16_t pixelA = CalcVertOffset(OscBufferA[oscBufferNumber][calculatedOffsetX]);
		const uint16_t pixelB = CalcVertOffset(OscBufferB[oscBufferNumber][calculatedOffsetX]) + calculatedOffsetYB;
		const uint16_t pixelC = CalcVertOffset(OscBufferC[oscBufferNumber][calculatedOffsetX]) + calculatedOffsetYC;

		// Starting a new screen: Set previous pixel to current pixel and clear frequency calculations
		if (drawPos == 0) {
			prevPixelA = pixelA;
			prevPixelB = pixelB;
			prevPixelC = pixelC;
			freqBelowZero = false;
			freqCrossZero = 0;
		}

		//	frequency calculation - detect upwards zero crossings
		const uint16_t currentChannelY = (config.oscDisplay & 1) ? OscBufferA[oscBufferNumber][calculatedOffsetX] :
										 (config.oscDisplay & 2) ? OscBufferB[oscBufferNumber][calculatedOffsetX] :
										  OscBufferC[oscBufferNumber][calculatedOffsetX];

		if (!freqBelowZero && currentChannelY < calibZeroPos) {		// first time reading goes below zero
			freqBelowZero = true;
		}
		if (freqBelowZero && currentChannelY >= calibZeroPos) {		// zero crossing
			//	second zero crossing - calculate frequency averaged over a number passes to smooth
			if (freqCrossZero > 0 && drawPos - freqCrossZero > 3) {
				if (freq > 0) {
					freq = (3 * freq + FreqFromPos(drawPos - freqCrossZero)) / 4;
				} else {
					freq = FreqFromPos(drawPos - freqCrossZero);
				}
			}
			freqCrossZero = drawPos;
			freqBelowZero = false;
		}

		// create draw buffer
		const std::pair<uint16_t, uint16_t> AY = std::minmax(pixelA, prevPixelA);
		const std::pair<uint16_t, uint16_t> BY = std::minmax(pixelB, prevPixelB);
		const std::pair<uint16_t, uint16_t> CY = std::minmax(pixelC, prevPixelC);

		const uint8_t vOffset = (drawPos < 27 || drawPos > 250) ? 11 : 0;		// offset draw area so as not to overwrite voltage and freq labels
		for (uint8_t h = 0; h <= lcd.drawHeight - (drawPos < 27 ? 12 : 0); ++h) {

			if (h < vOffset) {
				// do not draw
			} else if (config.oscDisplay & 1 && h >= AY.first && h <= AY.second) {
				lcd.drawBuffer[drawBufferNumber][h - vOffset] = LCD_GREEN;
			} else if (config.oscDisplay & 2 && h >= BY.first && h <= BY.second) {
				lcd.drawBuffer[drawBufferNumber][h - vOffset] = LCD_LIGHTBLUE;
			} else if (config.oscDisplay & 4 && h >= CY.first && h <= CY.second) {
				lcd.drawBuffer[drawBufferNumber][h - vOffset] = LCD_ORANGE;
			} else if (drawPos % 4 == 0 && (h + (lcd.drawHeight / (laneCount * 2))) % (lcd.drawHeight / (laneCount)) == 0) {						// 0v center mark
				lcd.drawBuffer[drawBufferNumber][h - vOffset] = LCD_GREY;
			} else {
				lcd.drawBuffer[drawBufferNumber][h - vOffset] = LCD_BLACK;
			}
		}

		// Draw grey lines indicating voltage range for one channel or channel divisions for 2 or 3 channels
		if (drawPos < 5) {
			for (int m = 1; m < (laneCount == 1 ? config.voltScale * 2 : (laneCount * 2)); ++m) {
				int vPos = m * lcd.drawHeight / (laneCount == 1 ? config.voltScale * 2 : (laneCount * 2)) - 11;
				if (vPos > 0) {
					lcd.drawBuffer[drawBufferNumber][vPos] = LCD_GREY;
				}
			}
		}

		lcd.PatternFill(drawPos, vOffset, drawPos, lcd.drawHeight - (drawPos < 27 ? 12 : 0), lcd.drawBuffer[drawBufferNumber]);
		drawBufferNumber = drawBufferNumber == 0 ? 1 : 0;

		// Store previous sample so next sample can be drawn as a line from old to new
		prevPixelA = pixelA;
		prevPixelB = pixelB;
		prevPixelC = pixelC;

		++drawPos;
		if (drawPos == lcd.drawWidth){
			drawing = false;
			noTriggerDraw = false;
		}

		// Draw trigger as a yellow cross
		if (drawPos == config.triggerX + 4) {
			const uint16_t vo = CalcVertOffset(config.triggerY) + (triggerTest == &adcB ? calculatedOffsetYB : triggerTest == &adcC ? calculatedOffsetYC : 0);
			if (vo > 4 && vo < lcd.drawHeight - 4) {
				lcd.DrawLine(config.triggerX, vo - 4, config.triggerX, vo + 4, LCD_YELLOW);
				lcd.DrawLine(std::max(config.triggerX - 4, 0), vo, config.triggerX + 4, vo, LCD_YELLOW);
			}
		}

		if (drawPos == 1) {
			// Write voltage
			lcd.DrawString(0, 1, " " + ui.IntToString(config.voltScale) + "v ", &lcd.Font_Small, LCD_GREY, LCD_BLACK);
			lcd.DrawString(0, lcd.drawHeight - 10, "-" + ui.IntToString(config.voltScale) + "v ", &lcd.Font_Small, LCD_GREY, LCD_BLACK);

			// Write frequency
			if (noTriggerDraw) {
				lcd.DrawString(250, 1, "No Trigger " , &lcd.Font_Small, LCD_WHITE, LCD_BLACK);
			} else {
				lcd.DrawString(250, 1, freq != 0 ? ui.FloatToString(freq, false) + "Hz    " : "          ", &lcd.Font_Small, LCD_WHITE, LCD_BLACK);
				freq = 0;
			}
		}

	}
}


uint16_t Osc::CalcVertOffset(const uint16_t& vPos)
{
	// Calculates vertical offset of oscilloscope trace from raw ADC value
	const float vOffset = (((static_cast<float>(vPos * config.vCalibScale + config.vCalibOffset) / (4.0f * 4096.0f) - 0.5f) * (8.0f / config.voltScale)) + 0.5f) / laneCount * lcd.drawHeight;
	return std::clamp(vOffset, 1.0f, static_cast<float>((lcd.drawHeight - 1) / laneCount));

}


float Osc::FreqFromPos(const uint16_t pos)
{
	// returns frequency of signal based on number of samples wide the signal is versus the sampling rate
	return static_cast<float>(SystemCoreClock) / (2.0f * pos * (TIM3->PSC + 1) * (TIM3->ARR + 1));
}


void Osc::setTriggerChannel()
{
	// Choose the trigger channel based on config preference and channel visibility
	triggerTest = config.triggerChannel == channelNone ? nullptr :
					 (config.triggerChannel == channelC && (config.oscDisplay & 4)) || config.oscDisplay == 4 ? &adcC :
					 (config.triggerChannel == channelB || !(config.oscDisplay & 1)) && (config.oscDisplay & 2) ? &adcB : &adcA;

}


void Osc::CalcZeroSize() {			// returns ADC size that corresponds to 0v
	calibZeroPos = (8192 - config.vCalibOffset) / config.vCalibScale;
}


uint32_t Osc::SerialiseConfig(uint8_t** buff)
{
	*buff = reinterpret_cast<uint8_t*>(&config);
	return sizeof(config);
}


uint32_t Osc::StoreConfig(uint8_t* buff)
{
	if (buff != nullptr) {
		memcpy(&config, buff, sizeof(config));
	}
	return sizeof(config);
}
