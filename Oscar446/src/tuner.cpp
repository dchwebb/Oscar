#include "tuner.h"

Tuner tuner;

void Tuner::Capture()
{
	if (mode == ZeroCrossing) {

		const uint32_t currVal = ADC_array[0] + ADC_array[3] + ADC_array[6] + ADC_array[9];
		++timer;

		if (overZero && currVal < calibZeroPos - 100) {
			overZero = false;
		}
		if (!overZero && currVal > calibZeroPos) {
			overZero = true;
			zeroCrossings[zCrossPos] = timer;
			if (++zCrossPos == zeroCrossings.size()) {
				TIM3->CR1 &= ~TIM_CR1_CEN;				// Disable the sample acquisiton timer
				samplesReady = true;
			}

		}
	}
}


void Tuner::Activate()
{
	if (mode == ZeroCrossing) {
		// Get current value of ADC (assume channel A for now)
		const uint32_t currVal = ADC_array[0] + ADC_array[3] + ADC_array[6] + ADC_array[9];
		overZero = currVal > calibZeroPos;
		timer = 0;
		zCrossPos = 0;
		TIM3->ARR = 1023;		// 90MHz / 1875 = 48kHz

		// Sample acquisition timer will be reactivated in ui.ResetMode
	}
}


void Tuner::Run()
{
	if (samplesReady) {
		float diff = zeroCrossings[2] - zeroCrossings[1];		// Get first time difference between zero crossings
		uint32_t matchCount = 0;
		samplesReady = false;

		for (uint32_t i = 2; i < zeroCrossings.size(); ++i) {
			float tempDiff = zeroCrossings[i + 1] - zeroCrossings[i];

			if (tempDiff - diff < 0.05f * diff) {
				++matchCount;
				diff = (diff + tempDiff) / 2;
			}
		}

		if (matchCount > 20) {
			float calcFreq = static_cast<float>(SystemCoreClock) / (2.0f * diff * (TIM3->PSC + 1) * (TIM3->ARR + 1));

			// if value is close apply some damping
			if (std::abs(calcFreq - currFreq) / calcFreq < 0.1f) {
				currFreq = 0.85f * currFreq + 0.15f * calcFreq;
			} else {
				currFreq = calcFreq;
			}

			lcd.DrawString(10, 10, currFreq != 0 ? ui.FloatToString(currFreq, false) + "Hz    " : "          ", &lcd.Font_Large, LCD_WHITE, LCD_BLACK);

		}
		Activate();
		TIM3->CR1 |= TIM_CR1_CEN;
	}
}
