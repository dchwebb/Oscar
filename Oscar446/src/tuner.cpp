#include "tuner.h"
#include "osc.h"

Tuner tuner;

Tuner::Tuner()
{
	samplesSize = sizeof(fft.fftBuffer) / 2;
	samples = (uint16_t*)&(fft.fftBuffer);

}

void Tuner::Capture()
{
	if (mode == ZeroCrossing) {

		const uint32_t currVal = ADC_array[0] + ADC_array[3] + ADC_array[6] + ADC_array[9];
		++timer;

		if (overZero && static_cast<int>(currVal) < calibZeroPos - 100) {
			overZero = false;
		}
		if (!overZero && currVal > calibZeroPos) {
			overZero = true;
			zeroCrossings[bufferPos] = timer;
			if (++bufferPos == zeroCrossings.size()) {
				samplesReady = true;
			}
		}

	} else if (mode == AutoCorrelation) {
		samples[bufferPos] = ADC_array[0] + ADC_array[3] + ADC_array[6] + ADC_array[9];
		if (++bufferPos > samplesSize) {
			samplesReady = true;
		}

	} else if (mode == FFT) {

		float* floatBuffer = (float*)&(fft.fftBuffer);

		const uint32_t adcSummed = ADC_array[0] + ADC_array[3] + ADC_array[6] + ADC_array[9];
		floatBuffer[bufferPos] = 2047.0f - (static_cast<float>(adcSummed) / 4.0f);

		// Capture 1.5 buffers - if capturing last third add it to the 2nd half of buffer 2
		// 1st half of buffer 2 will be populated later from 2nd half of buffer 1
		if (++bufferPos == FFT::fftSamples) {
			bufferPos += FFT::fftSamples / 2;
		}
		if (bufferPos > FFT::fftSamples * 2) {
			samplesReady = true;
		}
	}

	if (samplesReady) {
		TIM3->CR1 &= ~TIM_CR1_CEN;					// Disable the sample acquisiton timer
	}
}


void Tuner::Activate(bool startTimer)
{
	if (mode == ZeroCrossing) {
		// Get current value of ADC (assume channel A for now)
		const uint32_t currVal = ADC_array[0] + ADC_array[3] + ADC_array[6] + ADC_array[9];
		overZero = currVal > calibZeroPos;
		timer = 0;
		TIM3->ARR = zeroCrossRate;
	} else if (mode == AutoCorrelation) {
		TIM3->ARR = autoCorrRate;
	} else if (mode == FFT) {
		if (currFreq > 800) {
			TIM3->ARR = FFT::timerDefault / 2;
		} else {
			TIM3->ARR = FFT::timerDefault;
		}
	}

	bufferPos = 0;
	samplesReady = false;

	if (startTimer) {
		TIM3->CR1 |= TIM_CR1_CEN;
	}
}


char charBuff[100];

std::string FloatFmt(float f)
{
	sprintf(charBuff, "%.3f", f);
	return std::string(charBuff);
}


void Tuner::Run()
{
	if (samplesReady) {
		float frequency = 0.0f;
		const uint32_t start = SysTickVal;

		if (mode == ZeroCrossing) {
			float diff = zeroCrossings[1] - zeroCrossings[0];		// Get first time difference between zero crossings
			uint32_t stride = 1;			// Allow pitch detection where multiple zero crossings in cycle
			uint32_t noMatch = 0;			// When enough failed matches increase stride
			uint32_t matchCount = 0;

			uint32_t i;
			for (i = 1; i < zeroCrossings.size() - stride; ++i) {
				float tempDiff = zeroCrossings[i + stride] - zeroCrossings[i];

				if (tempDiff - diff < 0.05f * diff) {
					diff = (diff + tempDiff) / 2.0f;			// Apply some damping to average out differences
					++matchCount;
				} else {
					++noMatch;
					if (noMatch > 3 && stride < 10) {			// After three failures increase stride length and restart loop
						++stride;
						diff = zeroCrossings[stride] - zeroCrossings[0];
						i = 0;
						matchCount = 0;
						noMatch = 0;
					}
				}
			}

			if (matchCount > 10) {
				frequency = osc.FreqFromPos(diff);
			}

		} else if (mode == AutoCorrelation) {

			for (uint32_t lag = 0; lag < results.size(); ++lag) {
				float correlation = 0;
				for (uint32_t w = 0; w < window; ++w) {
					correlation += std::pow(samples[w + lag] - samples[w], 2);
				}
				results[lag] = static_cast<uint32_t>(correlation);
			}

			const uint32_t max = *std::max_element(results.begin(), results.end());		// Get largest value in array

			// Find the first item in the array that is 1% the size of the largest item
			const uint32_t threshold = max / 100;
			uint32_t dist = 0;
			uint32_t currLowest = 0;
			bool startSearch = false;			// Set to true once we are ready to search (as with zero lag auto-correlation is perfect)
			bool underThreshold = false;		// Used when locating lowest point below threshold
			for (; dist < results.size(); ++dist) {
				if (startSearch) {
					if (underThreshold) {
						if (results[dist] < currLowest) {
							currLowest = results[dist];
						} else {
							frequency = osc.FreqFromPos(dist - 1);
							break;
						}
					} else if (results[dist] < threshold) {
						underThreshold = true;
						currLowest = results[dist];
					}
				} else if (results[dist] > max / 90) {
					startSearch = true;
				}
			}
		} else if (mode == FFT) {
			// As we carry out two FFTs on samples 0 - 1023 then 512 - 1535, copy samples 512 - 1023 to position 1024
			memcpy(&(fft.fftBuffer[1]), &(fft.fftBuffer[0][512]), 512 * 4);

			// Carry out FFT on both buffers
			fft.CalcFFT(fft.fftBuffer[0], FFT::fftSamples);

			// Find first significant harmonic
			volatile uint32_t maxHyp = 0;
			volatile uint32_t maxBin = 0;
			bool localMax = false;

			for (uint32_t i = 1; i <= FFT::fftSamples / 2; ++i) {
				const float hypotenuse = std::hypot(fft.fftBuffer[0][i], fft.cosBuffer[i]);
				if (localMax) {
					if (hypotenuse > maxHyp) {
						maxHyp = hypotenuse;
						maxBin = i;
					} else {
						//nextHyp = hypotenuse;
						break;
					}
				} else if (hypotenuse > 50000) {
					localMax = true;
					maxHyp = hypotenuse;
					maxBin = i;
				}
			}
			if (maxBin) {

				const float phase0 = atan(fft.cosBuffer[maxBin] / fft.fftBuffer[0][maxBin]);

				// Carry out FFT on buffer 2
				fft.CalcFFT(fft.fftBuffer[1], FFT::fftSamples);

				const float phase1 = atan(fft.cosBuffer[maxBin] / fft.fftBuffer[1][maxBin]);
				float phaseAdj = phase0 - phase1;

				// handle phase wrapping
				if (phaseAdj < -M_PI / 2.0f) {
					phaseAdj += M_PI;
				}
				if (phaseAdj > M_PI / 2.0f) {
					phaseAdj -= M_PI;
				}

				// When a signal is almost exactly between two bins the first and second FFT can disagree
				// Use the direction of the phase adjustment to correct
				uint32_t hyp11 = std::hypot(fft.fftBuffer[1][maxBin + 0], fft.cosBuffer[maxBin + 0]);
				uint32_t hyp12 = std::hypot(fft.fftBuffer[1][maxBin + 1], fft.cosBuffer[maxBin + 1]);

				if (hyp12 > hyp11 && phaseAdj < 0.0f) {
					++maxBin;
				}

				const float adjIndex = static_cast<float>(maxBin) + phaseAdj / M_PI;

//				lcd.DrawString(10, 120, FloatFmt(phaseAdj) + " phase  ", &lcd.Font_XLarge, LCD_WHITE, LCD_BLACK);
//				lcd.DrawString(10, 170, FloatFmt(adjIndex) + " adjInd  ", &lcd.Font_XLarge, LCD_WHITE, LCD_BLACK);

				frequency = fft.HarmonicFreq(adjIndex);
			}
		}

		if (frequency > 0.0f) {

			// if value is close apply some damping; otherwise just use new value
			if (std::abs(frequency - currFreq) / frequency < 0.05f) {
				currFreq = 0.8f * currFreq + 0.2f * frequency;
			} else {
				currFreq = frequency;
			}
			lcd.DrawString(10, 10, ui.FloatToString(currFreq, false) + "Hz    ", &lcd.Font_XLarge, LCD_WHITE, LCD_BLACK);
			lcd.DrawString(10, 60, ui.IntToString(SysTickVal - start) + "ms  ", &lcd.Font_XLarge, LCD_WHITE, LCD_BLACK);

			// Formula for note is (ln(freq) - ln(16.35)) / ln(2 ^ (1/12))
			// Where 16.35 is frequency of low C and return value is semi-tones from low C
			constexpr float numRecip = 1.0f / log(pow(2.0f, 1.0f / 12.0f));
			constexpr float logBase = log(16.35160f);
			volatile float note = (log(currFreq) - logBase) * numRecip;
			volatile uint32_t pitch = std::lround(note) % 12;
			volatile uint32_t octave = std::lround(note) / 12;
			int32_t noteDiff = static_cast<int32_t>(100.0f * (note - std::round(note)));

			const std::string noteNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
			lcd.DrawString(10, 120, noteNames[pitch] + ui.IntToString(octave) + " " + ui.IntToString(noteDiff) + " cents   ", &lcd.Font_XLarge, LCD_WHITE, LCD_BLACK);

			// Calculate cent difference


		}


		Activate(true);
	}
}
