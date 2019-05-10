#include <fft.h>

fft::fft() {
	for (int s = 0; s < LUTSIZE; s++){
		SineLUT[s] = sin(s * 2.0f * M_PI / LUTSIZE);
	}
}

// Carry out Fast fourier transform
void fft::runFFT(volatile float candSin[]) {

	CP_ON

	int bitReverse = 0;

	if (std::isnan(candSin[0])) {
		int susp = 1;
	}

	// Bit reverse samples
	for (int i = 0; i < FFTSAMPLES; i++) {
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
	while (node < FFTSAMPLES) {

		if (node == 1) {

			// for the first loop the sine and cosine values will be 1 and 0 in all cases, simplifying the logic
			for (int p1 = 0; p1 < FFTSAMPLES; p1 += 2) {
				int p2 = p1 + node;

				float sinP2 = candSin[p2];

				candSin[p2] = candSin[p1] - sinP2;
				candCos[p2] = 0;
				candSin[p1] = candSin[p1] + sinP2;
				candCos[p1] = 0;
			}
		} else if (node == FFTSAMPLES / 2) {

			// last node - this only needs to calculate the first half of the FFT results as the remainder are redundant
			for (uint16_t p1 = 1; p1 < FFTSAMPLES; p1++) {

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
				for (int p1 = Wx; p1 < FFTSAMPLES; p1 += node * 2) {
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


	harmonic.fill(0);
	int16_t badFFT = 0, currHarmonic = -1, smearHarmonic = 0;
	maxHyp = 0;

	// display frequency spread
	std::string s = UI.intToString(std::round(harmonicFreq(1))) + " - " + UI.intToString(harmonicFreq(319)) + "Hz   ";
	lcd.DrawString(130, DRAWHEIGHT + 8, s, &lcd.Font_Small, LCD_WHITE, LCD_BLACK);

	// Draw results: Combine sine and cosines to get amplitudes and store in buffers, transmitting as each buffer is completed
	for (uint16_t i = 1; i <= DRAWWIDTH; i++) {
		uint16_t harmColour = LCD_BLUE;
		float hypotenuse = std::sqrt(std::pow(candSin[i], 2) + std::pow(candCos[i], 2));

		// get first four harmonics
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
					std::string harmonicInfo = UI.intToString(harmonicNumber) + " " + UI.floatToString(harmonicFreq(harmonic[h])) + "Hz";
					lcd.DrawStringMem(0, 20 + 20 * h, FFTDRAWBUFFERWIDTH, FFTDrawBuffer[FFTDrawBufferNumber], harmonicInfo, &lcd.Font_Small, harmColours[h], LCD_BLACK);

					debugCount = DMA2_Stream6->NDTR;			// tracks how many items left in DMA draw buffer
				}
			}
			lcd.PatternFill(i - FFTDRAWBUFFERWIDTH, 0, i - 1, DRAWHEIGHT, FFTDrawBuffer[FFTDrawBufferNumber]);
		}

	}

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

	CP_CAP

}

inline float fft::harmonicFreq(uint16_t harmonicNumber) {
	return ((float)SystemCoreClock * harmonicNumber) / (2 * FFTSAMPLES * (TIM3->PSC + 1) * (TIM3->ARR + 1));
}
