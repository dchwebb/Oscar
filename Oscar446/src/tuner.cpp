#include "tuner.h"

Tuner tuner;

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
			zeroCrossings[tunerPos] = timer;
			if (++tunerPos == zeroCrossings.size()) {
				TIM3->CR1 &= ~TIM_CR1_CEN;				// Disable the sample acquisiton timer
				samplesReady = true;
			}

		}
	} else if (mode == AutoCorrelation) {
		samples[tunerPos] = ADC_array[0] + ADC_array[3] + ADC_array[6] + ADC_array[9];
		if (++tunerPos > samples.size()) {
			TIM3->CR1 &= ~TIM_CR1_CEN;				// Disable the sample acquisiton timer
			samplesReady = true;
		}
	}
}


void Tuner::Activate(bool startTimer)
{
	if (mode == ZeroCrossing) {
		// Get current value of ADC (assume channel A for now)
		const uint32_t currVal = ADC_array[0] + ADC_array[3] + ADC_array[6] + ADC_array[9];
		overZero = currVal > calibZeroPos;
		timer = 0;
		TIM3->ARR = 1023;		// 90MHz / 1875 = 48kHz
	} else if (mode == AutoCorrelation) {

	}

	tunerPos = 0;
	samplesReady = false;

	if (startTimer) {
		TIM3->CR1 |= TIM_CR1_CEN;
	}
}


void Tuner::Run()
{
	if (samplesReady) {
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
				float calcFreq = static_cast<float>(SystemCoreClock) / (2.0f * diff * (TIM3->PSC + 1) * (TIM3->ARR + 1));

				// if value is close apply some damping
				if (std::abs(calcFreq - currFreq) / calcFreq < 0.1f) {
					currFreq = 0.85f * currFreq + 0.15f * calcFreq;
				} else {
					currFreq = calcFreq;
				}

				lcd.DrawString(10, 10, currFreq != 0 ? ui.FloatToString(currFreq, false) + "Hz    " : "          ", &lcd.Font_Large, LCD_WHITE, LCD_BLACK);

			}
		} else if (mode == AutoCorrelation) {

		}
		Activate(true);
	}
}
