#include <fft.h>

FFT::FFT() {
	for (int s = 0; s < LUTSIZE; s++){
		SineLUT[s] = sin(s * 2.0f * M_PI / LUTSIZE);
	}

	// clear the waterfall buffers
	for (uint16_t w = 0; w < WATERFALLBUFFERS; ++w) {
		for (uint16_t i = 0; i < WATERFALLSIZE; i++) {
			drawWaterfall[w][i]  = WATERFALLDRAWHEIGHT;
		}
	}
}

// Carry out Fast fourier transform
void FFT::runFFT(volatile float candSin[]) {

	CP_ON
	capture(candSin);
	displayFFT(candSin);
	CP_CAP
}

// Carry out Fast fourier transform
void FFT::waterfall(volatile float candSin[]) {

	CP_ON
	capture(candSin);
	displayWaterfall(candSin);
	CP_CAP
}

#define SMOOTHSIZE 4

inline float FFT::hypotenuse(volatile float sinArray[], uint16_t& pos) {
	return std::sqrt(std::pow(sinArray[pos], 2) + std::pow(candCos[pos], 2));
}

// Carry out Fast fourier transform
void FFT::displayWaterfall(volatile float candSin[]) {
	//if (waterfallBuffer == 0)
		//lcd.ScreenFill(LCD_BLACK);

	//lcd.DrawString(250, 1, ui.intToString(waterfallBuffer), &lcd.Font_Small, LCD_WHITE, LCD_BLACK);

	uint16_t hypPos = 0;
	uint16_t top, mult, div, sPos = 0;
	uint16_t smoothVals[SMOOTHSIZE];

	// Cycle through each column in the display and draw
	for (uint16_t i = 1; i < WATERFALLSIZE; i++) {
		// calculate hypotenuse ahead of draw position to apply smoothing
		while (hypPos < i + SMOOTHSIZE && hypPos < WATERFALLSIZE) {
			top = WATERFALLDRAWHEIGHT * (1 - (hypotenuse(candSin, hypPos) / (128 * WATERFALLSAMPLES)));
			drawWaterfall[waterfallBuffer][hypPos] = top;
			if (hypPos == 1) {
				drawWaterfall[waterfallBuffer][0] = top;
			}
			hypPos++;
		}


		// apply smoothing - uses binary weighting around center point eg: (1w(i-2) + 2w(i-1) + 4w(i) + 2w(i+1) + 1w(i+2)) / (1+2+4+2+1)
		top = div = 0;
		for (int16_t x = i - SMOOTHSIZE; x <= i + SMOOTHSIZE; x++) {
			if (x >= 0 && x < WATERFALLSIZE) {
				mult = 1 << (SMOOTHSIZE - std::abs(i - x));
				div += mult;
				top += mult * drawWaterfall[waterfallBuffer][x];
			}
		}
		top = top / div;

		// store smoothed values back to waterfallBuffer once all smoothing calculations have been done on that value
		smoothVals[sPos] = top;
		sPos = (sPos + 1) % SMOOTHSIZE;
		if (i >= SMOOTHSIZE) {
			drawWaterfall[waterfallBuffer][i - SMOOTHSIZE] = smoothVals[sPos];
		}


	}

/*
	// draw all readings
	// clear display buffer
	for (uint16_t b = 0; b < (DRAWHEIGHT + 1) * FFTDRAWBUFFERWIDTH; ++b) {
		FFTDrawBuffer[0][b] = LCD_BLACK;
		FFTDrawBuffer[1][b] = LCD_BLACK;
	}
*/

	/*// draw each buffer using separate lines (slow and flickery)
	uint8_t lastTop;
	for (uint16_t w = 0; w < WATERFALLBUFFERS; ++w) {
		// work backwards through the buffers so the newest buffer is drawn at the back and older buffers move forward
		int16_t buff = waterfallBuffer - w;

		if (buff < 0)	buff += WATERFALLBUFFERS;
		lastTop = drawWaterfall[buff][0];

		for (uint16_t i = 0; i < WATERFALLSIZE; i++) {

			int xOffset = 40 - w * 2;
			int yOffset = w * 5;

			lcd.DrawLine(i + xOffset, drawWaterfall[buff][i] + yOffset, i + xOffset, lastTop + yOffset, drawWaterfall[buff][i] > 95 ? LCD_LIGHTBLUE : LCD_WHITE);
			lcd.DrawLine(i + xOffset, std::max(drawWaterfall[buff][i], lastTop) + yOffset + 1, i + xOffset, WATERFALLDRAWHEIGHT + yOffset + 0, drawWaterfall[buff][i] > 95 ? LCD_BLUE : LCD_GREY);

			lastTop = drawWaterfall[buff][i];
		}

	}*/


	waterfallBuffer = (waterfallBuffer + 1) % WATERFALLBUFFERS;
	//lcd.ScreenFill(LCD_BLACK);


	// Cycle through each column in the display and draw
	for (uint16_t i = 1; i <= DRAWWIDTH; i++) {
		uint8_t FFTDrawBufferNumber = (((i - 1) / FFTDRAWBUFFERWIDTH) % 2 == 0) ? 0 : 1;
		uint16_t lastTop, vPos = DRAWHEIGHT;		// track the vertical position to apply blanking or skip drawing rows as required

		// work forwards through the buffers so the oldest buffer is drawn first at the front older buffers move forward
		for (uint16_t w = 0; w < WATERFALLBUFFERS; ++w) {

			int16_t buff = (waterfallBuffer + w) % WATERFALLBUFFERS;
			int xOffset = w * 2;
			int yOffset = (WATERFALLBUFFERS - w) * 5;

			// check that buffer is visible after applying offset
			if (i > xOffset && i < WATERFALLSIZE + xOffset) {

				top = drawWaterfall[buff][i - xOffset] + yOffset;
				lastTop = drawWaterfall[buff][i - xOffset - 1] + yOffset;

				while (vPos >= top  || vPos >= lastTop) {

					// draw column into memory buffer
					uint16_t buffPos = vPos * FFTDRAWBUFFERWIDTH + ((i - 1) % FFTDRAWBUFFERWIDTH);

					// depending on harmonic height draw either harmonic or black, using different colours to indicate main harmonics
					if (vPos > top && vPos > lastTop) {
						FFTDrawBuffer[FFTDrawBufferNumber][buffPos] = LCD_BLACK;
						//lcd.DrawPixel(i, h, LCD_BLACK);
					} else {
						FFTDrawBuffer[FFTDrawBufferNumber][buffPos] = LCD_GREEN;
						//lcd.DrawPixel(i, h, LCD_GREEN);
					}
					vPos--;

				}
			}
		}

		// black out any remaining pixels
		for (; vPos > 0; vPos--) {
			uint16_t buffPos = vPos * FFTDRAWBUFFERWIDTH + ((i - 1) % FFTDRAWBUFFERWIDTH);
			FFTDrawBuffer[FFTDrawBufferNumber][buffPos] = LCD_BLACK;
		}

		// check if ready to draw next buffer
		if ((i % FFTDRAWBUFFERWIDTH) == 0) {
			debugCount = DMA2_Stream6->NDTR;			// tracks how many items left in DMA draw buffer
			lcd.PatternFill(i - FFTDRAWBUFFERWIDTH, 0, i - 1, DRAWHEIGHT, FFTDrawBuffer[FFTDrawBufferNumber]);
		}
	}
}

// Carry out Fast fourier transform
void FFT::capture(volatile float candSin[]) {

	uint16_t bitReverse = 0;
	uint16_t FFTbits = log2(samples);

	// Bit reverse samples
	for (int i = 0; i < samples; i++) {
		// assembly bit reverses i and then rotates right to correct bit length
		asm("rbit %[result], %[value]\n\t"
			"ror %[result], %[shift]"
			: [result] "=r" (bitReverse) : [value] "r" (i), [shift] "r" (32 - FFTbits));

		if (bitReverse > i) {
			// bit reverse samples
			float temp = candSin[i];
			candSin[i] = candSin[bitReverse];
			candSin[bitReverse] = temp;
		}
	}


	// Step through each column in the butterfly diagram
	int node = 1;
	while (node < samples) {

		if (node == 1) {

			// for the first loop the sine and cosine values will be 1 and 0 in all cases, simplifying the logic
			for (int p1 = 0; p1 < samples; p1 += 2) {
				int p2 = p1 + node;

				float sinP2 = candSin[p2];

				candSin[p2] = candSin[p1] - sinP2;
				candCos[p2] = 0;
				candSin[p1] = candSin[p1] + sinP2;
				candCos[p1] = 0;
			}
		} else if (node == samples / 2) {

			// last node - this only needs to calculate the first half of the FFT results as the remainder are redundant
			for (uint16_t p1 = 1; p1 < samples / 2; p1++) {

				uint16_t b = std::round(p1 * LUTSIZE / (2 * node));
				float s = SineLUT[b];
				float c = SineLUT[b + LUTSIZE / 4 % LUTSIZE];

				int p2 = p1 + node;

				candSin[p1] += c * candSin[p2] - s * candCos[p2];
				candCos[p1] += c * candCos[p2] + s * candSin[p2];
			}

		} else {
			// Step through each value of the W function
			for (int Wx = 0; Wx < node; Wx++) {
				// Use Sine LUT to generate sine and cosine values faster than sine or cosine functions
				int b = std::round(Wx * LUTSIZE / (2 * node));
				float s = SineLUT[b];
				float c = SineLUT[b + LUTSIZE / 4 % LUTSIZE];

				// replace pairs of nodes with updated values
				for (int p1 = Wx; p1 < samples; p1 += node * 2) {
					int p2 = p1 + node;

					float sinP1 = candSin[p1];
					float cosP1 = candCos[p1];
					float sinP2 = candSin[p2];
					float cosP2 = candCos[p2];

					float t1 = c * sinP2 - s * cosP2;
					float t2 = c * cosP2 + s * sinP2;

					candSin[p2] = sinP1 - t1;
					candCos[p2] = cosP1 - t2;
					candSin[p1] = sinP1 + t1;
					candCos[p1] = cosP1 + t2;
				}
			}
		}
		node = node * 2;
	}
}


// Display results of FFT: Combine sine and cosines to get amplitudes and store in buffers, transmitting as each buffer is completed
void FFT::displayFFT(volatile float candSin[]) {

	harmonic.fill(0);
	int16_t badFFT = 0, currHarmonic = -1, smearHarmonic = 0;
	maxHyp = 0;

	// display frequency spread
	std::string s = ui.intToString(std::round(harmonicFreq(1))) + " - " + ui.intToString(harmonicFreq(319)) + "Hz   ";
	lcd.DrawString(130, DRAWHEIGHT + 8, s, &lcd.Font_Small, LCD_WHITE, LCD_BLACK);

	// Cycle through each column in the display and draw
	for (uint16_t i = 1; i <= DRAWWIDTH; i++) {
		uint16_t harmColour = LCD_BLUE;
		float hypotenuse = std::sqrt(std::pow(candSin[i], 2) + std::pow(candCos[i], 2));

		// get first few harmonics for colour coding and info
		if (currHarmonic < FFTHARMONICCOLOURS - 1 && hypotenuse > 50000) {
			if (currHarmonic == -1 || i > smearHarmonic + 1)
				currHarmonic++;

			smearHarmonic = i;			// used to display 'smeared' harmonics in the same colour -also avoids smeared harmonics showing as multiple harmonics
			harmColour = harmColours[currHarmonic];

			// check if current hypotenuse next one or is larger than previous one to shift harmonic up one
			if (harmonic[currHarmonic] == 0 || hypotenuse > maxHyp) {
				harmonic[currHarmonic] = i;
				maxHyp = hypotenuse;
			}
		} else {
			smearHarmonic = 0;
		}

		uint16_t top = std::min(DRAWHEIGHT * (1 - (hypotenuse / (512 * FFTSAMPLES))), (float)DRAWHEIGHT);

		uint8_t FFTDrawBufferNumber = (((i - 1) / FFTDRAWBUFFERWIDTH) % 2 == 0) ? 0 : 1;

		// draw column into memory buffer
		for (int h = 0; h <= DRAWHEIGHT; ++h) {
			uint16_t buffPos = h * FFTDRAWBUFFERWIDTH + ((i - 1) % FFTDRAWBUFFERWIDTH);

			// depending on harmonic height draw either harmonic or black, using different colours to indicate main harmonics
			if (h >= top) {
				badFFT++;					// every so often the FFT fails with extremely large numbers in all positions - just abort the draw and resample
				if (badFFT > 10000) {
					FFTErrors++;
					return;
				}
				FFTDrawBuffer[FFTDrawBufferNumber][buffPos] = harmColour;
			} else {
				FFTDrawBuffer[FFTDrawBufferNumber][buffPos] = LCD_BLACK;
			}
		}

		// check if ready to draw next buffer
		if ((i % FFTDRAWBUFFERWIDTH) == 0) {

			// if drawing the last buffer display the harmonic frequencies at the top right
			if (i > DRAWWIDTH - FFTDRAWBUFFERWIDTH) {
				for (uint8_t h = 0; h < FFTHARMONICCOLOURS; ++h) {
					if (harmonic[h] == 0)	break;

					uint16_t harmonicNumber = round((float)harmonic[h] / harmonic[0]);
					std::string harmonicInfo = ui.intToString(harmonicNumber) + " " + ui.floatToString(harmonicFreq(harmonic[h])) + "Hz";
					lcd.DrawStringMem(0, 20 + 20 * h, FFTDRAWBUFFERWIDTH, FFTDrawBuffer[FFTDrawBufferNumber], harmonicInfo, &lcd.Font_Small, harmColours[h], LCD_BLACK);

					debugCount = DMA2_Stream6->NDTR;			// tracks how many items left in DMA draw buffer
				}
			}
			lcd.PatternFill(i - FFTDRAWBUFFERWIDTH, 0, i - 1, DRAWHEIGHT, FFTDrawBuffer[FFTDrawBufferNumber]);
		}

	}

	// autotune attempts to lock the capture to an integer multiple of the fundamental for a clear display
	if (autoTune && harmonic[0] > 0) {
		freqFund = harmonicFreq(harmonic[0]);

		// work out which harmonic we want the fundamental to be - to adjust the sampling rate so a change in ARR affects the tuning of the FFT proportionally
		uint16_t targFund = std::max(std::round(freqFund / 10), 12.0f);

		// take the timer ARR, divide by fundamental to get new ARR setting tuned fundamental to target harmonic
		if (std::abs(targFund - harmonic[0]) > 1)	newARR = targFund * TIM3->ARR / harmonic[0];
		else										newARR = TIM3->ARR;

		//	fine tune - check the sample before and after the fundamental and adjust to center around the fundamental
		int sampleBefore = std::sqrt(pow(candSin[harmonic[0] - 1], 2) + pow(candCos[harmonic[0] - 1], 2));
		int sampleAfter  = std::sqrt(pow(candSin[harmonic[0] + 1], 2) + pow(candCos[harmonic[0] + 1], 2));

		// apply some hysteresis to avoid jumping around the target - the hysteresis needs to be scaled to the frequency
		if (sampleAfter > sampleBefore + 2 * freqFund * targFund) {
			diff = sampleAfter - sampleBefore;
			newARR -= 1;
		} else if (sampleBefore > sampleAfter + 2 * freqFund * targFund) {
			diff = sampleBefore - sampleAfter;
			newARR += 1;
		}

		if (newARR > 0 && newARR < 6000 && TIM3->ARR != newARR) {
			TIM3->ARR = newARR;
		}
	}



}

inline float FFT::harmonicFreq(uint16_t harmonicNumber) {
	return ((float)SystemCoreClock * harmonicNumber) / (2 * FFTSAMPLES * (TIM3->PSC + 1) * (TIM3->ARR + 1));
}
