#include "osc.h"

// Main loop to display oscilloscope trace
void Osc::OscRun()
{
	// check if we should start drawing
	if (!drawing && (capturing || noTriggerDraw)) {
		drawBufferNumber = noTriggerDraw ? !(bool)captureBufferNumber : captureBufferNumber;
		drawing = true;
		drawPos = 0;

		//	If in multi-lane mode get lane count from number of displayed channels and calculate vertical offset of channels B and C
		laneCount = (multiLane && oscDisplay == 0b111 ? 3 : multiLane && oscDisplay > 2 && oscDisplay != 4 ? 2 : 1);
		calculatedOffsetYB = (laneCount > 1 && oscDisplay & 0b001 ? lcd.drawHeight / laneCount : 0);
		calculatedOffsetYC = (laneCount == 2 ? lcd.drawHeight / 2 : laneCount == 3 ? lcd.drawHeight * 2 / 3 : 0);
		//CP_ON
	}

	// Check if drawing and that the sample capture is at or ahead of the draw position
	if (drawing && (drawBufferNumber != captureBufferNumber || capturedSamples[captureBufferNumber] >= drawPos || noTriggerDraw)) {
		// Calculate offset between capture and drawing positions to display correct sample
		uint16_t calculatedOffsetX = (drawOffset[drawBufferNumber] + drawPos) % lcd.drawWidth;


		uint16_t pixelA = CalcVertOffset(OscBufferA[drawBufferNumber][calculatedOffsetX]);
		uint16_t pixelB = CalcVertOffset(OscBufferB[drawBufferNumber][calculatedOffsetX]) + calculatedOffsetYB;
		uint16_t pixelC = CalcVertOffset(OscBufferC[drawBufferNumber][calculatedOffsetX]) + calculatedOffsetYC;

		// Starting a new screen: Set previous pixel to current pixel and clear frequency calculations
		if (drawPos == 0) {
			prevPixelA = pixelA;
			prevPixelB = pixelB;
			prevPixelC = pixelC;
			freqBelowZero = false;
			freqCrossZero = 0;
		}

		//	frequency calculation - detect upwards zero crossings
		uint16_t currentChannelY = 	(oscDisplay & 1) ? OscBufferA[drawBufferNumber][calculatedOffsetX] :
									(oscDisplay & 2) ? OscBufferB[drawBufferNumber][calculatedOffsetX] :
									OscBufferC[drawBufferNumber][calculatedOffsetX];

		if (!freqBelowZero && currentChannelY < CalibZeroPos) {		// first time reading goes below zero
			freqBelowZero = true;
		}
		if (freqBelowZero && currentChannelY >= CalibZeroPos) {		// zero crossing
			//	second zero crossing - calculate frequency averaged over a number passes to smooth
			if (freqCrossZero > 0 && drawPos - freqCrossZero > 3) {
				if (Freq > 0)
					Freq = (3 * Freq + FreqFromPos(drawPos - freqCrossZero)) / 4;
				else
					Freq = FreqFromPos(drawPos - freqCrossZero);
			}
			freqCrossZero = drawPos;
			freqBelowZero = false;
		}

		// create draw buffer
		std::pair<uint16_t, uint16_t> AY = std::minmax(pixelA, (uint16_t)prevPixelA);
		std::pair<uint16_t, uint16_t> BY = std::minmax(pixelB, (uint16_t)prevPixelB);
		std::pair<uint16_t, uint16_t> CY = std::minmax(pixelC, (uint16_t)prevPixelC);

		uint8_t vOffset = (drawPos < 27 || drawPos > 250) ? 11 : 0;		// offset draw area so as not to overwrite voltage and freq labels
		for (uint8_t h = 0; h <= lcd.drawHeight - (drawPos < 27 ? 12 : 0); ++h) {

			if (h < vOffset) {
				// do not draw
			} else if (oscDisplay & 1 && h >= AY.first && h <= AY.second) {
				DrawBuffer[DrawBufferNumber][h - vOffset] = LCD_GREEN;
			} else if (oscDisplay & 2 && h >= BY.first && h <= BY.second) {
				DrawBuffer[DrawBufferNumber][h - vOffset] = LCD_LIGHTBLUE;
			} else if (oscDisplay & 4 && h >= CY.first && h <= CY.second) {
				DrawBuffer[DrawBufferNumber][h - vOffset] = LCD_ORANGE;
			} else if (drawPos % 4 == 0 && (h + (lcd.drawHeight / (laneCount * 2))) % (lcd.drawHeight / (laneCount)) == 0) {						// 0v center mark
				DrawBuffer[DrawBufferNumber][h - vOffset] = LCD_GREY;
			} else {
				DrawBuffer[DrawBufferNumber][h - vOffset] = LCD_BLACK;
			}
		}

		// Draw grey lines indicating voltage range for one channel or channel divisions for 2 or 3 channels
		if (drawPos < 5) {
			for (int m = 1; m < (laneCount == 1 ? voltScale * 2 : (laneCount * 2)); ++m) {
				int test = m * lcd.drawHeight / (laneCount == 1 ? voltScale * 2 : (laneCount * 2)) - 11;
				DrawBuffer[DrawBufferNumber][test] = LCD_GREY;
			}
		}

		lcd.PatternFill(drawPos, vOffset, drawPos, lcd.drawHeight - (drawPos < 27 ? 12 : 0), DrawBuffer[DrawBufferNumber]);
		DrawBufferNumber = DrawBufferNumber == 0 ? 1 : 0;

		// Store previous sample so next sample can be drawn as a line from old to new
		prevPixelA = pixelA;
		prevPixelB = pixelB;
		prevPixelC = pixelC;

		drawPos ++;
		if (drawPos == lcd.drawWidth){
			drawing = false;
			noTriggerDraw = false;
			//CP_CAP
		}

		// Draw trigger as a yellow cross
		if (drawPos == TriggerX + 4) {
			uint16_t vo = CalcVertOffset(TriggerY) + (TriggerTest == &adcB ? calculatedOffsetYB : TriggerTest == &adcC ? calculatedOffsetYC : 0);
			if (vo > 4 && vo < lcd.drawHeight - 4) {
				lcd.DrawLine(TriggerX, vo - 4, TriggerX, vo + 4, LCD_YELLOW);
				lcd.DrawLine(std::max(TriggerX - 4, 0), vo, TriggerX + 4, vo, LCD_YELLOW);
			}
		}

		if (drawPos == 1) {
			// Write voltage
			lcd.DrawString(0, 1, " " + ui.intToString(voltScale) + "v ", &lcd.Font_Small, LCD_GREY, LCD_BLACK);
			lcd.DrawString(0, lcd.drawHeight - 10, "-" + ui.intToString(voltScale) + "v ", &lcd.Font_Small, LCD_GREY, LCD_BLACK);

			// Write frequency
			if (noTriggerDraw) {
				lcd.DrawString(250, 1, "No Trigger " , &lcd.Font_Small, LCD_WHITE, LCD_BLACK);
			} else {
				lcd.DrawString(250, 1, Freq != 0 ? ui.floatToString(Freq, false) + "Hz    " : "          ", &lcd.Font_Small, LCD_WHITE, LCD_BLACK);
				Freq = 0;
			}

		}

	}
}


// Main loop to display circular oscilloscope trace
void Osc::CircRun(){

	if ((!circDrawing[0] && circDataAvailable[0] && (circDrawPos[1] >= zeroCrossings[1] || !circDrawing[1])) ||
		(!circDrawing[1] && circDataAvailable[1] && (circDrawPos[0] >= zeroCrossings[0] || !circDrawing[0]))) {								// check if we should start drawing

		drawBufferNumber = (!circDrawing[0] && circDataAvailable[0] && (circDrawPos[1] == zeroCrossings[1] || !circDrawing[1])) ? 0 : 1;
		circDrawing[drawBufferNumber] = true;
		circDrawPos[drawBufferNumber] = 0;
		lcd.DrawString(140, lcd.drawHeight + 8, ui.floatToString(captureFreq[drawBufferNumber], true) + "Hz  ", &lcd.Font_Small, LCD_WHITE, LCD_BLACK);
		laneCount = 1;
	}

	// to have a continuous display drawing next sample as old sample is finishing
	for (drawBufferNumber = 0; drawBufferNumber < 2; drawBufferNumber++) {
		if (circDrawing[drawBufferNumber]) {

			// each of these loops will draw a trail from one draw buffer from current position (pos) up to end of previous draw position: circDrawPos[drawBufferNumber]
			for (int pos = std::max((int)circDrawPos[drawBufferNumber] - CIRCLENGTH, 0); pos <= circDrawPos[drawBufferNumber] && pos <= zeroCrossings[drawBufferNumber]; pos++) {

				int b = (int)std::round(pos * LUTSIZE / zeroCrossings[drawBufferNumber] + LUTSIZE / 4) % LUTSIZE;
				int x = fft.SineLUT[b] * 70 + 160;

				// draw a line getting fainter as it goes from current position backwards
				int pixelA = CalcVertOffset(OscBufferA[drawBufferNumber][pos]);
				if (pos == std::max((int)circDrawPos[drawBufferNumber] - CIRCLENGTH, 0)) {
					prevPixelA = pixelA;
				}

				uint16_t frontColour = ui.DarkenColour(fft.channel == channelA ? LCD_GREEN : fft.channel == channelB ? LCD_LIGHTBLUE : LCD_ORANGE,
						(circDrawPos[drawBufferNumber] - pos) / (fft.channel == channelA ? 3 : 4));
				uint16_t backColour = ui.DarkenColour(LCD_GREY, (circDrawPos[drawBufferNumber] - pos) / 8);

				if (pos < (int)circDrawPos[drawBufferNumber] - CIRCLENGTH + 2) {
					frontColour = LCD_BLACK;
					backColour = LCD_BLACK;
				}

				// Draw 'circle'
				lcd.DrawLine(x, pixelA, x, prevPixelA, frontColour);

				// Draw normal osc
				uint16_t oscPos = pos * lcd.drawWidth / zeroCrossings[drawBufferNumber];
				lcd.DrawLine(oscPos, pixelA, oscPos, prevPixelA, backColour);

				prevPixelA = pixelA;
			}

			circDrawPos[drawBufferNumber] ++;
			if (circDrawPos[drawBufferNumber] == zeroCrossings[drawBufferNumber] + CIRCLENGTH){
				circDrawing[drawBufferNumber] = false;
				circDataAvailable[drawBufferNumber] = false;
			}
		}
	}

}


// Calculates vertical offset of oscilloscope trace from raw ADC value
uint16_t Osc::CalcVertOffset(volatile const uint16_t& vPos)
{
	return std::max(std::min(((((float)(vPos * vCalibScale + vCalibOffset) / (4 * 4096) - 0.5f) * (8.0f / voltScale)) + 0.5f) / laneCount * lcd.drawHeight, (float)((lcd.drawHeight - 1) / laneCount)), 1.0f);
}


// returns frequency of signal based on number of samples wide the signal is in the screen
float Osc::FreqFromPos(const uint16_t pos)
{
	return (float)SystemCoreClock / (2.0f * pos * (TIM3->PSC + 1) * (TIM3->ARR + 1));
}

// Choose the trigger channel based on config preference and channel visibility
void Osc::setTriggerChannel()
{
	osc.TriggerTest = 	osc.TriggerChannel == channelNone ? nullptr :
						(osc.TriggerChannel == channelC && (osc.oscDisplay & 4)) || osc.oscDisplay == 4 ? &adcC :
						(osc.TriggerChannel == channelB || !(osc.oscDisplay & 1)) && (osc.oscDisplay & 2) ? &adcB : &adcA;

}
