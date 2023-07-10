#include "tuner.h"
#include "osc.h"


Tuner tuner;

extern  std::array<float, FFT::sinLUTSize> sineLUT;

void Tuner::Capture()
{
	const uint32_t adcSummed =  fft.config.channel == channelA ? ADC_array[0] + ADC_array[3] + ADC_array[6] + ADC_array[9] :
								fft.config.channel == channelB ? ADC_array[1] + ADC_array[4] + ADC_array[7] + ADC_array[10] :
														  	  	 ADC_array[2] + ADC_array[5] + ADC_array[8] + ADC_array[11];

	if (config.mode == FFT) {

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

		if (overZero && static_cast<int>(adcSummed) < osc.calibZeroPos - 100) {
			overZero = false;
		}
		if (!overZero && adcSummed > osc.calibZeroPos) {
			overZero = true;
			zeroCrossings[bufferPos] = timer;
			if (++bufferPos == zeroCrossings.size()) {
				samplesReady = true;
			}
		}

		// If no signal found abort and display no signal message
		if (timer > 200000) {
			samplesReady = true;
		}
	}

	if (samplesReady) {
		TIM3->CR1 &= ~TIM_CR1_CEN;					// Disable the sample acquisiton timer
	}
}


void Tuner::Activate(bool startTimer)
{
	if (config.mode == FFT) {
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
		overZero = currVal > osc.calibZeroPos;
		timer = 0;
		TIM3->ARR = zeroCrossRate;
	}

	bufferPos = 0;
	samplesReady = false;

	if (startTimer) {
		TIM3->CR1 |= TIM_CR1_CEN;
	}
}


std::pair<float, float> Tuner::FFTSingleBin(uint32_t bin)
{
	// Note this works pretty well but (probably due to rounding) gives slightly different answers to a full FFT
	float c = 0.0f;
	float s = 0.0f;
	for (uint32_t i = 0; i < FFT::fftSamples; ++i) {
		c += fft.fftBuffer[1][i] * fft.sinLUTExt[((i * bin) + (FFT::sinLUTSize / 4)) % FFT::sinLUTSize];
		s += fft.fftBuffer[1][i] * fft.sinLUTExt[(i * bin) % FFT::sinLUTSize];
	}

	return std::make_pair(c, s);
}


void Tuner::DrawOverlay()
{
	// Draw oscilloscope trace at bottom of tuner display
	const float trigger = 2047.0f - (static_cast<float>(osc.config.triggerY) / 4.0f);		// Normalise oscillator trigger to stored sample amplitude
	uint16_t start = 0;																// First sample where tigger activated
	float lastPoint = fft.fftBuffer[0][0];

	// Attempt to find trigger point
	for (uint16_t p = 0; p < FFT::fftSamples - lcd.drawWidth; p++) {
		if (lastPoint > trigger && fft.fftBuffer[0][p] < trigger) {
			start = p;
			break;
		}
		lastPoint = fft.fftBuffer[0][p];
	}

	uint16_t* overlayDrawBuffer = &lcd.drawBuffer[0][0];					// lcd draw buffer is wrong dimensions for overlay

	const uint16_t overlayColour = fft.config.channel == channelA ? LCD_DULLGREEN : fft.config.channel == channelB ? LCD_DULLBLUE : LCD_DULLORANGE;

	memset(overlayDrawBuffer, 0, overlayHeight * lcd.drawWidth * 2);		// Clear draw buffer (*2 as buffer is uint16_t)

	uint32_t currVPos = OverlayVPos(fft.fftBuffer[0][start]);				// Used to fill in vertical lines between columns
	for (uint16_t p = 0; p < lcd.drawWidth ; ++p) {
		const uint32_t vPos = OverlayVPos(fft.fftBuffer[0][start + p]);
		do {
			currVPos += currVPos < vPos ? 1 : currVPos > vPos ? -1 : 0;
			overlayDrawBuffer[currVPos * lcd.drawWidth + p] = overlayColour;
		} while (vPos != currVPos);
	}

	// Overlay is actually drawn at end of tuning process so display can be updated while next capture underway
}


inline uint32_t Tuner::OverlayVPos(float sample)
{
	// Stored samples are amplitude +/-2047
	const float scaled = (((sample / 4096.0f) * (8.0f / osc.config.voltScale)) + 0.5f) * overlayHeight;
	return static_cast<uint32_t>(std::clamp(scaled, 0.0f, overlayHeight));

}


void Tuner::ClearOverlay()
{
	lcd.ColourFill(0, 108 + 1, lcd.drawWidth - 1, lcd.drawHeight, LCD_BLACK);
}


void Tuner::Run()
{
	if (samplesReady) {
		float frequency = 0.0f;
		const uint32_t start = SysTickVal;

		if (config.mode == FFT) {
			// Phase adjusted FFT: FFT on two overlapping buffers; where the 2nd half of buffer 1 is the 1st part of buffer 2
			// Calculate the fundamental bin looking for a magnitude over a threshold, then adjust by the phase difference of the two FFTs

			// As we do two FFTs on samples 0 - 1023 then 512 - 1535, copy samples 512 - 1023 to position 1024
			memcpy(&(fft.fftBuffer[1]), &(fft.fftBuffer[0][512]), 512 * 4);

			// Capture samples for overlay
			if (config.traceOverlay) {
				DrawOverlay();
			}

			fft.CalcFFT(fft.fftBuffer[0], FFT::fftSamples);			// Carry out FFT on first buffer

			// Find first significant harmonic
			uint32_t fundMag = 0;
			uint32_t maxMag = 0;
			uint32_t maxBin = 0;
			bool localMax = false;			// True once magnitude of bin is large enough to count as fundamental

			// Locate maximum hypoteneuse
			for (uint32_t i = 1; i < FFT::fftSamples / 2; ++i) {
				const float hypotenuse = std::hypot(fft.fftBuffer[0][i], fft.cosBuffer[i]);
				if (hypotenuse > maxMag) {
					maxMag = hypotenuse;
				}
			}

			// Locate first hypoteuse that is large enough relative to the maximum to count as fundamental
			for (uint32_t i = 1; i < FFT::fftSamples / 2; ++i) {
				const float hypotenuse = std::hypot(fft.fftBuffer[0][i], fft.cosBuffer[i]);
				if (localMax) {
					if (hypotenuse > fundMag) {
						fundMag = hypotenuse;
						maxBin = i;
					} else {
						break;
					}
				} else if (hypotenuse > magThreshold && hypotenuse > static_cast<float>(maxMag) * 0.5f) {
					localMax = true;
					fundMag = hypotenuse;
					maxBin = i;
				}
			}

			if (maxBin) {

				const float phase0 = atan(fft.cosBuffer[maxBin] / fft.fftBuffer[0][maxBin]);

				fft.CalcFFT(fft.fftBuffer[1], FFT::fftSamples);				// Carry out FFT on buffer 2 (overwrites cosine results from first FFT)

				const float phase1 = atan(fft.cosBuffer[maxBin] / fft.fftBuffer[1][maxBin]);
				float phaseAdj = (phase0 - phase1) / M_PI;			// normalise phase adjustment

				// handle phase wrapping
				if (phaseAdj < -0.5f) {
					phaseAdj += 1.0f;
				}
				if (phaseAdj > 0.5f) {
					phaseAdj -= 1.0f;
				}

				// When a signal is almost exactly between two bins the first and second FFT can disagree
				// Use the direction of the phase adjustment to correct
				const uint32_t hyp11 = std::hypot(fft.fftBuffer[1][maxBin + 0], fft.cosBuffer[maxBin + 0]);
				const uint32_t hyp12 = std::hypot(fft.fftBuffer[1][maxBin + 1], fft.cosBuffer[maxBin + 1]);
				if (hyp12 > hyp11 && phaseAdj < 0.0f) {		// Correct for situations where each FFT disagrees about the fundamental bin
					++maxBin;
				}

				frequency = fft.HarmonicFreq(static_cast<float>(maxBin) + phaseAdj);

				// Possibly due to rounding errors at around 50% phase adjustments the cycle rate can be out by a cycle - abort and shift sampling rate
				if (phaseAdj > 0.47f || phaseAdj < -0.47f) {
					if (std::fabs(frequency - currFreq) / currFreq > 0.05f) {
						sampleRateAdj += currFreq < 60 ? 32 : 2;
						frequency = 0.0f;
					}
				}

			}

		} else {
			// Zero crossing mode
			if (bufferPos == zeroCrossings.size()) {						// Check that a signal was found

				float diff = zeroCrossings[1] - zeroCrossings[0];		// Get first time difference between zero crossings
				uint32_t stride = 1;									// Allow pitch detection where multiple zero crossings in cycle
				uint32_t noMatch = 0;									// When enough failed matches increase stride
				uint32_t matchCount = 0;

				uint32_t i;
				for (i = 1; i < zeroCrossings.size() - stride; ++i) {
					float tempDiff = zeroCrossings[i + stride] - zeroCrossings[i];

					if (tempDiff - diff < 0.05f * diff) {
						diff = (diff + tempDiff) / 2.0f;				// Apply some damping to average out differences
						++matchCount;
					} else {
						++noMatch;
						if (noMatch > 3 && stride < 10) {				// After three failures increase stride length and restart loop
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
		}

		if (frequency > 16.35f) {
			// if value is close apply some damping; otherwise just use new value
			if (std::abs(frequency - currFreq) / frequency < 0.04f) {
				currFreq = 0.7f * currFreq + 0.3f * frequency;
			} else {
				currFreq = frequency;
			}

			// Formula to get musical note from frequency is (ln(freq) - ln(16.35)) / ln(2 ^ (1/12))
			// Where 16.35 is frequency of low C and return value is semi-tones from low C
			const std::string noteNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
			constexpr float numRecip = 1.0f / log(pow(2.0f, 1.0f / 12.0f));		// Store reciprocal to avoid division
			constexpr float logBase = log(16.35160f);
			float note = (log(currFreq) - logBase) * numRecip;
			const uint32_t pitch = std::lround(note) % 12;
			const uint32_t octave = std::lround(note) / 12;
			const int32_t centDiff = static_cast<int32_t>(100.0f * (note - std::round(note)));

			// Draw note name and octave with cent error to the left (-) or right (+)
			lcd.DrawString(30, 35, centDiff < 0 ? ui.IntToString(centDiff) + "   ": "     ", &lcd.Font_XLarge, LCD_WHITE, LCD_BLACK);
			lcd.DrawString(110, 35, noteNames[pitch] + ui.IntToString(octave) + "  ", &lcd.Font_XLarge, LCD_WHITE, LCD_BLACK);
			lcd.DrawString(170, 35, centDiff > 0 ? "+" + ui.IntToString(centDiff) + " ": "    ", &lcd.Font_XLarge, LCD_WHITE, LCD_BLACK);

			const uint16_t hertzColour = fft.config.channel == channelA ? LCD_GREEN : fft.config.channel == channelB ? LCD_LIGHTBLUE : LCD_ORANGE;
			lcd.DrawString(80, 85, ui.FloatToString(currFreq, false) + "Hz    ", &lcd.Font_XLarge, hertzColour, LCD_BLACK);

			convBlink = !convBlink;
			lcd.ColourFill(300, 5, 305, 10, convBlink ? hertzColour : LCD_BLACK);

			// Debug timing
			//lcd.DrawString(10, 180, ui.IntToString(SysTickVal - start) + "ms   ", &lcd.Font_Large, LCD_GREY, LCD_BLACK);
			lcd.DrawString(140, lcd.drawHeight + 8, ui.IntToString(SysTickVal - start) + "ms   ", &lcd.Font_Small, LCD_WHITE, LCD_BLACK);

			lastValid = SysTickVal;

		} else if (SysTickVal - lastValid > 4000) {
			lcd.DrawString(30, 35, "  No Signal    ", &lcd.Font_XLarge, LCD_GREY, LCD_BLACK);
			lcd.DrawString(80, 85, ui.FloatToString(currFreq, false) + "Hz    ", &lcd.Font_XLarge, LCD_GREY, LCD_BLACK);
		}

		// Draw overlay here whilst next data is being captured
		if (config.traceOverlay) {
			while (SPI_DMA_Working);
			lcd.PatternFill(0, 108 + 1, lcd.drawWidth - 1, lcd.drawHeight, &lcd.drawBuffer[0][0]);
		}

		Activate(true);
	}
}


uint32_t Tuner::SerialiseConfig(uint8_t** buff)
{
	*buff = reinterpret_cast<uint8_t*>(&config);
	return sizeof(config);
}


uint32_t Tuner::StoreConfig(uint8_t* buff)
{
	if (buff != nullptr) {
		memcpy(&config, buff, sizeof(config));
	}
	return sizeof(config);
}
