#include "tuner.h"

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
		TIM3->ARR = FFT::timerDefault;
	}

	bufferPos = 0;
	samplesReady = false;

	if (startTimer) {
		TIM3->CR1 |= TIM_CR1_CEN;
	}
}


void Tuner::Run()
{
	if (samplesReady) {
		uint32_t freqInSamples = 0;
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
				freqInSamples = diff;
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
							freqInSamples = dist - 1;
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

			volatile int susp = 1;
		}

		if (freqInSamples) {
			const float calcFreq = static_cast<float>(SystemCoreClock) / (2.0f * freqInSamples * (TIM3->PSC + 1) * (TIM3->ARR + 1));

			// if value is close apply some damping; otherwise just use new value
			if (std::abs(calcFreq - currFreq) / calcFreq < 0.1f) {
				currFreq = 0.9f * currFreq + 0.1f * calcFreq;
			} else {
				currFreq = calcFreq;
			}
			lcd.DrawString(10, 10, ui.FloatToString(currFreq, false) + "Hz    ", &lcd.Font_XLarge, LCD_WHITE, LCD_BLACK);
			lcd.DrawString(10, 60, ui.IntToString(SysTickVal - start) + "ms  ", &lcd.Font_XLarge, LCD_WHITE, LCD_BLACK);
		}


		Activate(true);
	}
}
