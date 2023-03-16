#include <fft.h>

FFT::FFT()
{
	for (uint32_t s = 0; s < sinLUTSize; ++s){
		sineLUT[s] = sin(s * 2.0f * M_PI / sinLUTSize);
	}

	// clear the waterfall buffers
	for (uint16_t w = 0; w < waterfallBuffers; ++w) {
		for (uint16_t i = 0; i < waterfallSize; i++) {
			drawWaterfall[w][i] = waterfallDrawHeight;
		}
	}
}


void FFT::Run()
{
	// Carry out Fast fourier transform
	sampleCapture(false);									// checks if ready to start new capture

	if (dataAvailable[0] || dataAvailable[1]) {
		fftBufferNumber = dataAvailable[0] ? 0 : 1;			// select correct draw buffer based on whether buffer 0 or 1 contains data
		calcFFT(fftBuffer[fftBufferNumber]);

		if (displayMode == Fourier) {
			displayFFT(fftBuffer[fftBufferNumber]);
		} else {
			displayWaterfall(fftBuffer[fftBufferNumber]);
		}
	}
}


void FFT::sampleCapture(const bool clearBuffer)
{
	// Starts new fft sample capture
	if (clearBuffer) {
		dataAvailable[fftBufferNumber] = false;
	}

	if (!capturing && (!dataAvailable[0] || !dataAvailable[1])) {
		capturing = true;
		capturePos = 0;
		captureBufferIndex = dataAvailable[0] ? 1 : 0;
	}
}


void FFT::calcFFT(float* sinBuffer)
{
	// Carry out Fast fourier transform
	uint16_t bitReverse = 0;
	const uint16_t fftbits = log2(samples);

	// Populate draw buffer to overlay sample view
	if (traceOverlay) {
		osc.laneCount = 2;

		// attempt to find if there is a trigger point
		uint16_t s = 0;
		uint16_t t = 4 * (2047 - sinBuffer[0]);
		for (uint16_t p = 0; p < samples - lcd.drawWidth; p++) {
			if (t > osc.triggerY && (4 * (2047 - sinBuffer[p])) < osc.triggerY) {
				s = p;
				break;
			}
			t = 4 * (2047 - sinBuffer[p]);
		}

		for (uint16_t p = 0; p < lcd.drawWidth ; ++p) {
			adcA = 4 * (2047 - sinBuffer[s + p]);
			osc.OscBufferA[0][p] = osc.CalcVertOffset(adcA) + (lcd.drawHeight / 4);
		}
		osc.prevPixelA = osc.OscBufferA[0][0];
	}

	// Bit reverse samples
	for (int i = 0; i < samples; ++i) {
		// assembly bit reverses i and then rotates right to correct bit length
		asm("rbit %[result], %[value]\n\t"
			"ror %[result], %[shift]"
			: [result] "=r" (bitReverse) : [value] "r" (i), [shift] "r" (32 - fftbits));

		if (bitReverse > i) {
			// bit reverse samples
			const float temp = sinBuffer[i];
			sinBuffer[i] = sinBuffer[bitReverse];
			sinBuffer[bitReverse] = temp;
		}
	}


	// Step through each column in the butterfly diagram
	int node = 1;
	while (node < samples) {

		if (node == 1) {

			// for the first loop the sine and cosine values will be 1 and 0 in all cases, simplifying the logic
			for (uint32_t p1 = 0; p1 < samples; p1 += 2) {
				const uint32_t p2 = p1 + node;
				const float sinP2 = sinBuffer[p2];
				sinBuffer[p2] = sinBuffer[p1] - sinP2;
				cosBuffer[p2] = 0;
				sinBuffer[p1] = sinBuffer[p1] + sinP2;
				cosBuffer[p1] = 0;
			}

		} else if (node == samples / 2) {

			// last node - this only needs to calculate the first half of the FFT results as the remainder are redundant
			for (uint32_t p1 = 1; p1 < samples / 2; ++p1) {
				const uint16_t b = std::round(p1 * sinLUTSize / (2 * node));
				const float s = sineLUT[b];
				const float c = sineLUT[b + sinLUTSize / 4 % sinLUTSize];

				const int p2 = p1 + node;

				sinBuffer[p1] += c * sinBuffer[p2] - s * cosBuffer[p2];
				cosBuffer[p1] += c * cosBuffer[p2] + s * sinBuffer[p2];
			}

		} else {
			// All but first and last nodes: step through each value of the W function
			for (int Wx = 0; Wx < node; ++Wx) {

				// Use Sine LUT to generate sine and cosine values faster than sine or cosine functions
				const int b = std::round(Wx * sinLUTSize / (2 * node));
				const float s = sineLUT[b];
				const float c = sineLUT[b + sinLUTSize / 4 % sinLUTSize];

				// replace pairs of nodes with updated values
				for (int p1 = Wx; p1 < samples; p1 += node * 2) {
					int p2 = p1 + node;

					const float sinP1 = sinBuffer[p1];
					const float cosP1 = cosBuffer[p1];
					const float sinP2 = sinBuffer[p2];
					const float cosP2 = cosBuffer[p2];

					const float t1 = c * sinP2 - s * cosP2;
					const float t2 = c * cosP2 + s * sinP2;

					sinBuffer[p2] = sinP1 - t1;
					cosBuffer[p2] = cosP1 - t2;
					sinBuffer[p1] = sinP1 + t1;
					cosBuffer[p1] = cosP1 + t2;
				}
			}
		}
		node = node * 2;
	}

	// Display frequency spread
	lcd.DrawString(115, lcd.drawHeight + 8, ui.FloatToString(harmonicFreq(1), true) + " - " + ui.FloatToString(harmonicFreq(319), true) + "Hz  ",
			&lcd.Font_Small, LCD_WHITE, LCD_BLACK);

}


// Display results of FFT: Combine sine and cosines to get amplitudes and store in buffers, transmitting as each buffer is completed
void FFT::displayFFT(const float* sinBuffer)
{
	harmonic.fill(0);
	int16_t badFFT = 0, currHarmonic = -1, smearHarmonic = 0;
	uint32_t maxHyp = 0;

	const uint16_t overlayColour = fft.channel == channelA ? LCD_DULLGREEN : fft.channel == channelB ? LCD_DULLBLUE : LCD_DULLORANGE;

	// Cycle through each column in the display and draw
	for (uint16_t i = 1; i <= lcd.drawWidth; i++) {
		uint16_t harmColour = LCD_BLUE;
		const float hypotenuse = std::hypot(sinBuffer[i], cosBuffer[i]);

		// get first few harmonics for colour coding and info
		if (currHarmonic < fftHarmonicColours - 1 && hypotenuse > 50000) {
			if (currHarmonic == -1 || i > smearHarmonic + 1) {
				currHarmonic++;
			}

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

		const uint16_t top = std::min(lcd.drawHeight * (1.0f - (hypotenuse / (512.0f * fftSamples))), static_cast<float>(lcd.drawHeight));

		const uint8_t fftDrawBufferNumber = (((i - 1) / lcd.drawBufferWidth) % 2 == 0) ? 0 : 1;

		// draw column into memory buffer
		for (uint32_t h = 0; h <= lcd.drawHeight; ++h) {
			uint16_t buffPos = h * lcd.drawBufferWidth + ((i - 1) % lcd.drawBufferWidth);

			const std::pair<uint16_t, uint16_t> AY = std::minmax(osc.OscBufferA[0][i - 1], osc.prevPixelA);		// Index i is offset by 1 from start of buffer

			// depending on harmonic height draw either harmonic or black, using different colours to indicate main harmonics
			if (h >= top) {
				badFFT++;					// every so often the FFT fails with extremely large numbers in all positions - just abort the draw and resample
				if (badFFT > 10000) {
					fftErrors++;
					dataAvailable[fftBufferNumber] = false;
					return;
				}
				lcd.drawBuffer[fftDrawBufferNumber][buffPos] = harmColour;

			} else if (traceOverlay && h >= AY.first && h <= AY.second) {		// Draw oscilloscope trace as overlay
				lcd.drawBuffer[fftDrawBufferNumber][buffPos] = overlayColour;

			} else {
				lcd.drawBuffer[fftDrawBufferNumber][buffPos] = LCD_BLACK;
			}
		}

		osc.prevPixelA = osc.OscBufferA[0][i - 1];

		// check if ready to draw next buffer
		if (i % lcd.drawBufferWidth == 0) {

			// if drawing the last buffer display the harmonic frequencies at the top right
			if (i > lcd.drawWidth - lcd.drawBufferWidth) {
				sampleCapture(true);			// Signal to Interrupt that new capture can start

				for (uint8_t h = 0; h < fftHarmonicColours; ++h) {
					if (harmonic[h] == 0) {
						break;
					}

					const float freq = harmonicFreq(harmonic[h]);

					if (h == 0) {
						// Draw first harmonic in larger font
						std::string harmonicInfo = ui.FloatToString(freq, false) + "Hz";
						lcd.DrawStringMem(0, 20, lcd.drawBufferWidth, lcd.drawBuffer[fftDrawBufferNumber], harmonicInfo, &lcd.Font_Large, LCD_WHITE, LCD_BLACK);
					} else {
						const uint16_t harmonicNumber = round(static_cast<float>(harmonic[h]) / harmonic[0]);
						std::string harmonicInfo = ui.IntToString(harmonicNumber) + " " + ui.FloatToString(freq, false) + "Hz";
						lcd.DrawStringMem(0, 30 + 20 * h, lcd.drawBufferWidth, lcd.drawBuffer[fftDrawBufferNumber], harmonicInfo, &lcd.Font_Small, harmColours[h], LCD_BLACK);
					}
				}
			}
			lcd.PatternFill(i - lcd.drawBufferWidth, 0, i - 1, lcd.drawHeight, lcd.drawBuffer[fftDrawBufferNumber]);
		}
	}

	// autotune attempts to lock the capture to an integer multiple of the fundamental for a clear display
	if (autoTune && harmonic[0] > 0) {
		const float freqFund = harmonicFreq(harmonic[0]);

		// work out which harmonic we want the fundamental to be - to adjust the sampling rate so a change in ARR affects the tuning of the FFT proportionally
		const float targFund = std::max(freqFund / 10, 8.0f);
		float newARR;

		// take the timer ARR, divide by fundamental to get new ARR setting tuned fundamental to target harmonic
		if (std::abs(targFund - harmonic[0]) > 1) {
			newARR = targFund * TIM3->ARR / harmonic[0];
		} else {
			newARR = TIM3->ARR;			// Difference is within one so only use fine adjustment
		}

		//	fine tune - check the sample before and after the fundamental and adjust to center around the fundamental
		float sampleBefore = std::hypot(sinBuffer[harmonic[0] - 1], cosBuffer[harmonic[0] - 1]);
		float sampleAfter  = std::hypot(sinBuffer[harmonic[0] + 1], cosBuffer[harmonic[0] + 1]);

		// Change ARR by an amount related to the proportion of the difference of the two surrounding harmonics,
		// scaling faster at lower frequencies where there is more resolution
		if (sampleAfter + sampleBefore > 100.0f) {
			newARR += ((sampleAfter - sampleBefore) / (sampleAfter + sampleBefore)) * (freqFund < 80 ? -60.0f :  -30.0f);
		}

		if (newARR > 0.0f) {
			TIM3->ARR = std::round(newARR);
		}
	}
}


inline float FFT::harmonicFreq(const uint16_t harmonicNumber)
{
	return static_cast<float>(SystemCoreClock) * harmonicNumber / (2.0f * fftSamples * (TIM3->PSC + 1) * (TIM3->ARR + 1));
}


void FFT::displayWaterfall(const float* sinBuffer)
{
	uint16_t badFFT = 0, top, mult, div, sPos = 0, hypPos = 0;
	uint16_t smoothVals[waterfallSmooth];

	// Cycle through each column in the display and draw
	for (uint16_t i = 1; i < waterfallSize; i++) {
		// calculate hypotenuse ahead of draw position to apply smoothing
		while (hypPos < i + waterfallSmooth && hypPos < waterfallSize) {
			top = waterfallDrawHeight * (1 - (std::hypot(sinBuffer[hypPos], cosBuffer[hypPos]) / (128 * WATERFALLSAMPLES)));

			if (top < 30) {
				badFFT++;					// every so often the FFT fails with extremely large numbers in all positions - just abort the draw and resample
				if (badFFT > 200) {
					fftErrors++;
					dataAvailable[fftBufferNumber] = false;
					return;
				}
			}

			drawWaterfall[waterfallBuffer][hypPos] = top;
			if (hypPos == 1) {
				drawWaterfall[waterfallBuffer][0] = top;
			}
			hypPos++;
		}

		// apply smoothing - uses binary weighting around center point eg: (1w(i-2) + 2w(i-1) + 4w(i) + 2w(i+1) + 1w(i+2)) / (1+2+4+2+1)
		top = div = 0;
		for (int16_t x = i - waterfallSmooth; x <= i + waterfallSmooth; x++) {
			if (x >= 0 && x < waterfallSize) {
				mult = 1 << (waterfallSmooth - std::abs(i - x));
				div += mult;
				top += mult * drawWaterfall[waterfallBuffer][x];
			}
		}
		top = top / div;

		// store smoothed values back to waterfallBuffer once all smoothing calculations have been done on that value
		smoothVals[sPos] = top;
		sPos = (sPos + 1) % waterfallSmooth;
		if (i >= waterfallSmooth) {
			drawWaterfall[waterfallBuffer][i - waterfallSmooth] = smoothVals[sPos];
		}

	}

	sampleCapture(true);			// Signal to Interrupt that new capture can start

	waterfallBuffer = (waterfallBuffer + 1) % waterfallBuffers;

	int16_t h0, h1;

	// Cycle through each column in the display and draw
	for (uint16_t col = 1; col <= lcd.drawWidth; ++col) {
		const uint8_t fftDrawBufferNumber = (((col - 1) / lcd.drawBufferWidth) % 2 == 0) ? 0 : 1;
		int16_t vPos = lcd.drawHeight;		// track the vertical position to apply blanking or skip drawing rows as required

		// work forwards through the buffers so the oldest buffer is drawn first at the front, newer buffers move forward
		for (uint16_t w = 0; w < waterfallBuffers; ++w) {

			//	Darken green has less effect than darkening orange or blue - adjust accordingly
			const uint16_t colourShade = ui.DarkenColour(fft.channel == channelA ? LCD_GREEN : fft.channel == channelB ? LCD_LIGHTBLUE : LCD_ORANGE,  (uint16_t)w * 2 * (fft.channel == channelA ? 1 : 0.8));

			const int16_t buff = (waterfallBuffer + w) % waterfallBuffers;
			int xOffset = w * 2 + 3;
			int yOffset = (waterfallBuffers - w) * 5 - 12;

			// check that buffer is visible after applying offset
			if (col > xOffset && col < waterfallSize + xOffset) {

				h1 = drawWaterfall[buff][col - xOffset] + yOffset;
				h0 = drawWaterfall[buff][col - xOffset - 1] + yOffset;

				while (vPos > 0 && (vPos >= h1 || vPos >= h0)) {

					// draw column into memory buffer
					uint16_t buffPos = vPos * lcd.drawBufferWidth + ((col - 1) % lcd.drawBufferWidth);

					// depending on harmonic height draw either green or black, using darker shades of green at the back
					if (vPos > h1 && vPos > h0) {
						lcd.drawBuffer[fftDrawBufferNumber][buffPos] = LCD_BLACK;
					} else {
						lcd.drawBuffer[fftDrawBufferNumber][buffPos] = colourShade;
					}
					--vPos;
				}
			}
		}

		// black out any remaining pixels
		for (; vPos >= 0; --vPos) {
			const uint16_t buffPos = vPos * lcd.drawBufferWidth + ((col - 1) % lcd.drawBufferWidth);
			lcd.drawBuffer[fftDrawBufferNumber][buffPos] = LCD_BLACK;
		}

		// check if ready to draw next buffer
		if ((col % lcd.drawBufferWidth) == 0) {
			lcd.PatternFill(col - lcd.drawBufferWidth, 0, col - 1, lcd.drawHeight, lcd.drawBuffer[fftDrawBufferNumber]);
		}
	}
}
