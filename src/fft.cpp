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

	/*
	// create an test array to transform
	for (int i = 0; i < FFTSAMPLES; i++) {
		// Sine Wave + harmonic
		candSin[i] = 4096 * (sin(2.0f * M_PI * i / FFTSAMPLES) + (1.0f / harm) * sin(harm * 2.0f * M_PI * i / FFTSAMPLES));
		//candSin[i] = 4096 * ((2.0f * (FFTSAMPLES - i) / FFTSAMPLES) - 1);	// Saw Tooth
		//candSin[i] = i < (FFTSAMPLES / 2) ? 2047 : -2047;					// Square wave
	}*/

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
			for (uint16_t p1 = 1; p1 <= FFTSAMPLES; p1++) {

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

	fundHarmonic = 0;
	int badFFT = 0;
	harmonic2 = harmonic3 = harmonic4 = 0;

	// Draw results: Combine sine and cosines to get amplitudes and store in buffers, transmitting as each buffer is completed
	for (uint16_t i = 1; i <= DRAWWIDTH; i++) {

		float hypotenuse = std::sqrt(std::pow(candSin[i], 2) + std::pow(candCos[i], 2));

		// get fundamental harmonic
		if (fundHarmonic == 0 && hypotenuse > 200000) {
			maxHyp = hypotenuse;
			fundHarmonic = i;

			// calculate the frequency of the fundamental
			freqFund = ((float)SystemCoreClock * fundHarmonic) / (2 * FFTSAMPLES * (TIM3->PSC + 1) * (TIM3->ARR + 1));

			// write fundamental frequency to display
			std::string s = UI.floatToString(freqFund) + "Hz   ";
			lcd.DrawString(140, DRAWHEIGHT + 8, s, &lcd.Font_Small, LCD_WHITE, LCD_BLACK);
		}

		// get next three harmonics
		if (fundHarmonic > 0 && hypotenuse > 50000 && harmonic4 == 0 && i > fundHarmonic + 1) {
			if (harmonic2 == 0)				harmonic2 = i;
			else if (harmonic3 == 0) {
				if (i > harmonic2 + 1)		harmonic3 = i;
			}
			else if (i > harmonic3 + 1)		harmonic4 = i;
		}

		uint16_t top = std::min(DRAWHEIGHT * (1 - (hypotenuse / (512 * FFTSAMPLES))), (float)DRAWHEIGHT);

		uint8_t FFTDrawBufferNumber = (((i - 1) / FFTDRAWBUFFERSIZE) % 2 == 0) ? 0 : 1;

		// draw column into memory buffer
		for (int h = 0; h <= DRAWHEIGHT; ++h) {
			uint16_t buffPos = h * FFTDRAWBUFFERSIZE + ((i - 1) % FFTDRAWBUFFERSIZE);
			uint16_t harmColour = LCD_BLACK;
			// use different colours to indicate different harmonics

			if (h >= top) {
				if (fundHarmonic == i)			harmColour = LCD_WHITE;
				else if (harmonic2 == i)		harmColour = LCD_YELLOW;
				else if (harmonic3 == i)		harmColour = LCD_ORANGE;
				else if (harmonic4 == i)		harmColour = LCD_GREEN;
				else							harmColour = LCD_BLUE;

				badFFT++;					// every so often the FFT fails with extremely large numbers in all positions - just abort the draw and resample
				if (badFFT > 10000)
					return;
			}
			FFTDrawBuffer[FFTDrawBufferNumber][buffPos] = harmColour;

		}

		// check if ready to draw next buffer
		if ((i % FFTDRAWBUFFERSIZE) == 0) {
			lcd.PatternFill(i - FFTDRAWBUFFERSIZE, 0, i - 1, DRAWHEIGHT, FFTDrawBuffer[FFTDrawBufferNumber]);
		}

	}

	if (autoTune && fundHarmonic > 0) {


		// work out which harmonic we want the fundamental to be - to adjust the sampling rate so a change in ARR affects the tuning of the FFT proportionally
		uint16_t targFund = std::max(std::round(freqFund / 10), 8.0f);

		// take the timer ARR, divide by fundamental to get new ARR setting tuned fundamental to target harmonic
		if (std::abs(targFund - fundHarmonic) > 1)	newARR = targFund * TIM3->ARR / fundHarmonic;
		else										newARR = TIM3->ARR;

		//	fine tune - check the sample before and after the fundamental and adjust to center around the fundamental
		int sampleBefore = std::sqrt(candSin[fundHarmonic - 1] * candSin[fundHarmonic - 1] + candCos[fundHarmonic - 1] * candCos[fundHarmonic - 1]);
		int sampleAfter = std::sqrt(candSin[fundHarmonic + 1] * candSin[fundHarmonic + 1] + candCos[fundHarmonic + 1] * candCos[fundHarmonic + 1]);

		// apply some hysteresis to avoid jumping around the target - the hysteresis needs to be scaled to the frequency
		if (sampleAfter > sampleBefore + 2 * freqFund * targFund) {
			diff = sampleAfter - sampleBefore;
			newARR -= 1;
		} else if (sampleBefore > sampleAfter + 2 * freqFund * targFund) {
			diff = sampleBefore - sampleAfter;
			newARR += 1;
		}

		if (newARR > 0 && newARR < 6000) {
			TIM3->ARR = newARR;

		}

	}
	debugCount = DMA2_Stream6->NDTR;
	CP_CAP
}
