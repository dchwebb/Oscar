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

			// last node - this draws samples rather than calculate them
			for (uint16_t p1 = 1; p1 <= DRAWWIDTH; p1++) {
				// Use Sine LUT to generate sine and cosine values faster than sine or cosine functions
				uint16_t b = std::round(p1 * LUTSIZE / (2 * node));
				float s = SineLUT[b];
				float c = SineLUT[b + LUTSIZE / 4 % LUTSIZE];

				int p2 = p1 + node;

				// true if drawing after FFT calculations
				if (FFTDRAWAFTERCALC) {
					candSin[p1] += c * candSin[p2] - s * candCos[p2];
					candCos[p1] += c * candCos[p2] + s * candSin[p2];
				} else {

					// Combine final node calculation sine and cosines to get amplitudes and store in alternate buffers, transmitting as each buffer is completed
					float hypotenuse = std::sqrt(std::pow(candSin[p1] + (c * candSin[p2] - s * candCos[p2]), 2) + std::pow(candCos[p1] + (c * candCos[p2] + s * candSin[p2]), 2));
					uint16_t top = std::min(DRAWHEIGHT * (1 - (hypotenuse / (512 * FFTSAMPLES))), (float)DRAWHEIGHT);

					uint8_t FFTDrawBufferNumber = (((p1 - 1) / FFTDRAWBUFFERSIZE) % 2 == 0) ? 0 : 1;		// Alternate between buffer 0 and buffer 1

					// draw column into memory buffer
					for (int h = 0; h <= DRAWHEIGHT; ++h) {
						uint16_t buffPos = h * FFTDRAWBUFFERSIZE + ((p1 - 1) % FFTDRAWBUFFERSIZE);
						FFTDrawBuffer[FFTDrawBufferNumber][buffPos] = (h < top || p1 >= node) ? LCD_BLACK: LCD_BLUE;
					}

					// check if ready to draw next buffer
					if ((p1 % FFTDRAWBUFFERSIZE) == 0) {
						lcd.PatternFill(p1 - FFTDRAWBUFFERSIZE, 0, p1 - 1, DRAWHEIGHT, FFTDrawBuffer[FFTDrawBufferNumber]);
					}
				}
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

	if (FFTDRAWAFTERCALC) {
		// Combine sine and cosines to get amplitudes and store in alternate buffers, transmitting as each buffer is completed
		for (uint16_t i = 1; i <= std::min(FFTSAMPLES / 2, DRAWWIDTH); i++) {

			float hypotenuse = std::sqrt(std::pow(candSin[i], 2) + std::pow(candCos[i], 2));
			uint16_t top = std::min(DRAWHEIGHT * (1 - (hypotenuse / (512 * FFTSAMPLES))), (float)DRAWHEIGHT);

			uint8_t FFTDrawBufferNumber = (((i - 1) / FFTDRAWBUFFERSIZE) % 2 == 0) ? 0 : 1;

			// draw column into memory buffer
			for (int h = 0; h <= DRAWHEIGHT; ++h) {
				uint16_t buffPos = h * FFTDRAWBUFFERSIZE + ((i - 1) % FFTDRAWBUFFERSIZE);
				FFTDrawBuffer[FFTDrawBufferNumber][buffPos] = h < top ? LCD_BLACK: LCD_BLUE;

				if (top < 10 && i > 100) {
					int susp = 1;
				}
			}

			// check if ready to draw next buffer
			if ((i % FFTDRAWBUFFERSIZE) == 0) {
				debugCount = DMA2_Stream6->NDTR;
				lcd.PatternFill(i - FFTDRAWBUFFERSIZE, 0, i - 1, DRAWHEIGHT, FFTDrawBuffer[FFTDrawBufferNumber]);
			}
		}
	}

	CP_CAP
}
