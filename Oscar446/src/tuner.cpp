#include "tuner.h"
#include "osc.h"

Tuner tuner;

Tuner::Tuner()
{
//	samplesSize = sizeof(fft.fftBuffer) / 2;
//	samples = (uint16_t*)&(fft.fftBuffer);

}


void Tuner::Capture()
{
	const uint32_t adcSummed =  fft.channel == channelA ? ADC_array[0] + ADC_array[3] + ADC_array[6] + ADC_array[9] :
								fft.channel == channelB ? ADC_array[1] + ADC_array[4] + ADC_array[7] + ADC_array[10] :
														  ADC_array[2] + ADC_array[5] + ADC_array[8] + ADC_array[11];

	if (mode == FFT) {

		float* floatBuffer = (float*)&(fft.fftBuffer);
		floatBuffer[bufferPos] = 2047.0f - (static_cast<float>(adcSummed) / 4.0f);

		// Capture 1.5 buffers - if capturing last third add it to the 2nd half of buffer 2
		// 1st half of buffer 2 will be populated later from 2nd half of buffer 1
		if (++bufferPos == FFT::fftSamples) {
			bufferPos += FFT::fftSamples / 2;
		}
		if (bufferPos > FFT::fftSamples * 2) {
			samplesReady = true;
		}
	} else {
		++timer;

		if (overZero && static_cast<int>(adcSummed) < calibZeroPos - 100) {
			overZero = false;
		}
		if (!overZero && adcSummed > calibZeroPos) {
			overZero = true;
			zeroCrossings[bufferPos] = timer;
			if (++bufferPos == zeroCrossings.size()) {
				samplesReady = true;
			}
		}
	}

	if (samplesReady) {
		TIM3->CR1 &= ~TIM_CR1_CEN;					// Disable the sample acquisiton timer
	}
}


void Tuner::Activate(bool startTimer)
{
	if (mode == FFT) {
		if (currFreq > 800.0f) {
			TIM3->ARR = (FFT::timerDefault / 2) + sampleRateAdj;
		} else if (currFreq < 50.0f) {
			TIM3->ARR = (FFT::timerDefault * 2) + sampleRateAdj;
		} else {
			TIM3->ARR = FFT::timerDefault + sampleRateAdj;
		}
	} else {
		// Get current value of ADC (assume channel A for now)
		const uint32_t currVal = ADC_array[0] + ADC_array[3] + ADC_array[6] + ADC_array[9];
		overZero = currVal > calibZeroPos;
		timer = 0;
		TIM3->ARR = zeroCrossRate;
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
		float frequency = 0.0f;
		const uint32_t start = SysTickVal;

		if (mode == FFT) {
			// As we do two FFTs on samples 0 - 1023 then 512 - 1535, copy samples 512 - 1023 to position 1024
			memcpy(&(fft.fftBuffer[1]), &(fft.fftBuffer[0][512]), 512 * 4);

			// Carry out FFT on both buffers
			fft.CalcFFT(fft.fftBuffer[0], FFT::fftSamples);

			// Find first significant harmonic
			volatile uint32_t maxMag = 0;
			volatile uint32_t maxBin = 0;
			bool localMax = false;			// True once magnitude of bin is large enough to count as fundamental

			for (uint32_t i = 1; i <= FFT::fftSamples / 2; ++i) {
				const float hypotenuse = std::hypot(fft.fftBuffer[0][i], fft.cosBuffer[i]);
				if (localMax) {
					if (hypotenuse > maxMag) {
						maxMag = hypotenuse;
						maxBin = i;
					} else {
						break;
					}
				} else if (hypotenuse > 50000) {
					localMax = true;
					maxMag = hypotenuse;
					maxBin = i;
				}
			}

			if (maxBin) {

				volatile const float phase0 = atan(fft.cosBuffer[maxBin] / fft.fftBuffer[0][maxBin]);

				fft.CalcFFT(fft.fftBuffer[1], FFT::fftSamples);				// Carry out FFT on buffer 2

				volatile const float phase1 = atan(fft.cosBuffer[maxBin] / fft.fftBuffer[1][maxBin]);
				volatile float phaseAdj = (phase0 - phase1) / M_PI;			// normalise phase adjustment

				// handle phase wrapping
				if (phaseAdj < -0.5f) {
					phaseAdj += 1.0f;
				}
				if (phaseAdj > 0.5f) {
					phaseAdj -= 1.0f;
				}

				// When a signal is almost exactly between two bins the first and second FFT can disagree
				// Use the direction of the phase adjustment to correct
				volatile const uint32_t hyp11 = std::hypot(fft.fftBuffer[1][maxBin + 0], fft.cosBuffer[maxBin + 0]);
				volatile const uint32_t hyp12 = std::hypot(fft.fftBuffer[1][maxBin + 1], fft.cosBuffer[maxBin + 1]);

				if (hyp12 > hyp11 && phaseAdj < 0.0f) {
					++maxBin;
				}

				// Possibly due to rounding errors at around 50% phase adjustments the cycle rate can be out by a cycle - abort and shift sampling rate
				if (phaseAdj > 0.48f) {
					++sampleRateAdj;
				} else {
					frequency = fft.HarmonicFreq(static_cast<float>(maxBin) + phaseAdj);
				}
			}
		} else {
			// Zero crossing mode

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

		}

		if (frequency > 0.0f) {

			// if value is close apply some damping; otherwise just use new value
			if (std::abs(frequency - currFreq) / frequency < 0.05f) {
				currFreq = 0.7f * currFreq + 0.3f * frequency;
			} else {
				currFreq = frequency;
			}
		}

		if (currFreq > 0.0f) {
			const uint16_t fontColour = frequency > 0.0f ? LCD_WHITE : LCD_GREY;

			lcd.DrawString(10, 10, ui.FloatToString(currFreq, false) + "Hz    ", &lcd.Font_XLarge, fontColour, LCD_BLACK);

			// Formula to get musical note from frequency is (ln(freq) - ln(16.35)) / ln(2 ^ (1/12))
			// Where 16.35 is frequency of low C and return value is semi-tones from low C
			constexpr float numRecip = 1.0f / log(pow(2.0f, 1.0f / 12.0f));		// Store reciprocal to avoid division
			constexpr float logBase = log(16.35160f);
			float note = (log(currFreq) - logBase) * numRecip;
			uint32_t pitch = std::lround(note) % 12;
			uint32_t octave = std::lround(note) / 12;
			int32_t centDiff = static_cast<int32_t>(100.0f * (note - std::round(note)));

			const std::string noteNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
			lcd.DrawString(10, 80, noteNames[pitch] + ui.IntToString(octave) + " " + ui.IntToString(centDiff) + " cents   ", &lcd.Font_XLarge, fontColour, LCD_BLACK);
		} else {
			lcd.DrawString(10, 10, "No Signal    ", &lcd.Font_XLarge, LCD_GREY, LCD_BLACK);
		}



		Activate(true);
	}
}


