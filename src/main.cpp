#include "stm32f4xx.h"
#include <cmath>
#include <string>
#include <queue>
#include "initialisation.h"
#include "ui.h"
#include "lcd.h"
#include "fft.h"
#include "midi.h"
#include "osc.h"

extern uint32_t SystemCoreClock;

volatile uint16_t OscBufferA[2][DRAWWIDTH], OscBufferB[2][DRAWWIDTH], OscBufferC[2][DRAWWIDTH];
volatile uint16_t prevPixelA = 0, prevPixelB = 0, prevPixelC = 0, adcA, adcB, adcC, oldAdc, capturePos = 0, drawPos = 0, bufferSamples = 0;
volatile bool freqBelowZero, capturing = false, drawing = false, encoderBtnL = false, encoderBtnR = false;
volatile uint8_t VertOffsetA = 0, VertOffsetB = 0, captureBufferNumber = 0, drawBufferNumber = 0;
volatile int8_t encoderPendingL = 0, encoderStateL = 0, encoderPendingR = 0, encoderStateR = 0;
volatile int16_t drawOffset[2] {0, 0};
volatile uint16_t capturedSamples[2] {0, 0};
volatile uint32_t debugCount = 0, coverageTimer = 0, coverageTotal = 0;
volatile float freqFund;
volatile uint16_t ADC_array[ADC_BUFFER_LENGTH];
volatile uint16_t freqCrossZero, FFTErrors = 0;


//	default calibration values for 15k and 100k resistors on input opamp scaling to a maximum of 8v (slightly less for negative signals)
volatile int16_t vCalibOffset = -4190;
volatile float vCalibScale = 1.24f;
volatile int8_t voltScale = 8;
volatile uint16_t CalibZeroPos = 9985;

volatile uint16_t zeroCrossings[2] {0, 0};
volatile bool circDrawing[2] {false, false};
volatile uint16_t circDrawPos[2] {0, 0};
volatile uint16_t circPrevPixel[2] {0, 0};

volatile bool circDataAvailable[2] {false, false};
volatile float captureFreq[2] {0, 0};
volatile float circAngle;
mode displayMode = Oscilloscope;
#define CIRCLENGTH 160


LCD lcd;
FFT fft;
UI ui;
MIDIHandler midi;
Osc osc;



inline uint16_t CalcVertOffset(volatile const uint16_t& vPos) {
	return std::max(std::min(((((float)(vPos * vCalibScale + vCalibOffset) / (4 * 4096) - 0.5f) * (8.0f / voltScale)) + 0.5f) * DRAWHEIGHT, (float)DRAWHEIGHT - 1), 1.0f);
}

inline uint16_t CalcZeroSize() {					// returns ADC size that corresponds to 0v
	return (8192 - vCalibOffset) / vCalibScale;
}

inline float FreqFromPos(const uint16_t pos) {		// returns frequency of signal based on number of samples wide the signal is in the screen
	return (float)SystemCoreClock / (2.0f * pos * (TIM3->PSC + 1) * (TIM3->ARR + 1));
}

//	Interrupts: Use extern C to allow linker to find ISR
extern "C"
{
	// Main sample capture
	void TIM3_IRQHandler(void) {

		TIM3->SR &= ~TIM_SR_UIF;					// clear UIF flag

		if (displayMode == Fourier || displayMode == Waterfall) {
			if (capturePos == fft.samples && capturing) {
				fft.dataAvailable[captureBufferNumber] = true;
				capturing = false;
			}

			if (capturing) {
				// For FFT Mode we want a value between +- 2047
				if (fft.channel == channelA)
					fft.FFTBuffer[captureBufferNumber][capturePos] = 2047 - ((float)(ADC_array[0] + ADC_array[3] + ADC_array[6] + ADC_array[9]) / 4);
				else if (fft.channel == channelB)
					fft.FFTBuffer[captureBufferNumber][capturePos] = 2047 - ((float)(ADC_array[1] + ADC_array[4] + ADC_array[7] + ADC_array[10]) / 4);
				else if (fft.channel == channelC)
					fft.FFTBuffer[captureBufferNumber][capturePos] = 2047 - ((float)(ADC_array[2] + ADC_array[5] + ADC_array[8] + ADC_array[11]) / 4);
				capturePos ++;
			}

		} else if (displayMode == Circular) {
			// Average the last four ADC readings to smooth noise
			if (fft.channel == channelA)
				adcA = ADC_array[0] + ADC_array[3] + ADC_array[6] + ADC_array[9];
			else if (fft.channel == channelB)
				adcA = ADC_array[1] + ADC_array[4] + ADC_array[7] + ADC_array[10];
			else
				adcA = ADC_array[2] + ADC_array[5] + ADC_array[8] + ADC_array[11];


			// check if we should start capturing - ie there is a buffer spare and a zero crossing has occured
			if (!capturing && oldAdc < CalibZeroPos && adcA >= CalibZeroPos && (!circDataAvailable[0] || !circDataAvailable[1])) {
				capturing = true;
				captureBufferNumber = circDataAvailable[0] ? 1 : 0;		// select correct capture buffer based on whether buffer 0 or 1 contains data
				capturePos = 0;				// used to check if a sample is ready to be drawn
				zeroCrossings[captureBufferNumber] = 0;
			}

			// If capturing store current readings in buffer and increment counters
			if (capturing) {
				OscBufferA[captureBufferNumber][capturePos] = adcA;

				// store array of zero crossing points
				if (capturePos > 0 && oldAdc < CalibZeroPos && adcA >= CalibZeroPos) {
					zeroCrossings[captureBufferNumber] = capturePos;
					circDataAvailable[captureBufferNumber] = true;

					captureFreq[captureBufferNumber] = FreqFromPos(capturePos);		// get frequency here before potentially altering sampling speed
					capturing = false;

					// auto adjust sample time to try and get the longest sample for the display (280 is number of pixels wide we ideally want the captured wave to be)
					if (capturePos < 280) {
						int16_t newARR = capturePos * (TIM3->ARR + 1) / 280;
						if (newARR > 0)
							TIM3->ARR = newARR;
					}

				// reached end  of buffer and zero crossing not found - increase timer size to get longer sample
				} else if (capturePos == DRAWWIDTH - 1) {
					capturing = false;
					TIM3->ARR += 30;

				} else {
					capturePos++;
				}
			}
			oldAdc = adcA;

		} else if (displayMode == Oscilloscope) {
			// Average the last four ADC readings to smooth noise
			/*adcA = ADC_array[0] + ADC_array[2] + ADC_array[4] + ADC_array[6];
			adcB = ADC_array[1] + ADC_array[3] + ADC_array[5] + ADC_array[7];
*/
			adcA = ADC_array[0] + ADC_array[3] + ADC_array[6] + ADC_array[9];
			adcB = ADC_array[1] + ADC_array[4] + ADC_array[7] + ADC_array[10];
			adcC = ADC_array[2] + ADC_array[5] + ADC_array[8] + ADC_array[11];

			// check if we should start capturing - ie not drawing from the capture buffer and crossed over the trigger threshold (or in free mode)
			if (!capturing && (!drawing || captureBufferNumber != drawBufferNumber) && (osc.TriggerTest == nullptr || (bufferSamples > osc.TriggerX && oldAdc < osc.TriggerY && *osc.TriggerTest >= osc.TriggerY))) {
				capturing = true;

				if (osc.TriggerTest == nullptr) {										// free running mode
					capturePos = 0;
					drawOffset[captureBufferNumber] = 0;
					capturedSamples[captureBufferNumber] = -1;
				} else {
					// calculate the drawing offset based on the current capture position minus the horizontal trigger position
					drawOffset[captureBufferNumber] = capturePos - osc.TriggerX;
					if (drawOffset[captureBufferNumber] < 0)	drawOffset[captureBufferNumber] += DRAWWIDTH;

					capturedSamples[captureBufferNumber] = osc.TriggerX - 1;	// used to check if a sample is ready to be drawn
				}
			}

			// if capturing check if write buffer is full and switch to next buffer if so; if not full store current reading
			if (capturing && capturedSamples[captureBufferNumber] == DRAWWIDTH - 1) {
				captureBufferNumber = captureBufferNumber == 1 ? 0 : 1;		// switch the capture buffer
				bufferSamples = 0;			// stores number of samples captured since switching buffers to ensure triggered mode works correctly
				capturing = false;
			}

			// If capturing or buffering samples waiting for trigger store current readings in buffer and increment counters
			if (capturing || !drawing || captureBufferNumber != drawBufferNumber) {
				OscBufferA[captureBufferNumber][capturePos] = adcA;
				OscBufferB[captureBufferNumber][capturePos] = adcB;
				OscBufferC[captureBufferNumber][capturePos] = adcC;
				oldAdc = *osc.TriggerTest;

				if (capturePos == DRAWWIDTH - 1)	capturePos = 0;
				else								capturePos++;

				if (capturing)	capturedSamples[captureBufferNumber]++;
				else 			bufferSamples++;

			}
		}
	}

	// Left Encoder Button
	void EXTI4_IRQHandler(void) {

		if (!(GPIOE->IDR & GPIO_IDR_IDR_4)) 						// Encoder button PE4 pressed
			DB_ON													// Enable debounce timer
		if (GPIOE->IDR & GPIO_IDR_IDR_4 && TIM5->CNT > 100) {		// Encoder button released - check enough time has elapsed to ensure not a bounce. A quick press if around 300, a long one around 8000+
			encoderBtnL = true;
			DB_OFF													// Disable debounce timer
		}
		EXTI->PR |= EXTI_PR_PR4;									// Clear interrupt pending
	}

	// Right Encoder
	void EXTI9_5_IRQHandler(void) {

		if (EXTI->PR & EXTI_PR_PR7) {
			if (!(GPIOA->IDR & GPIO_IDR_IDR_7)) 					// Encoder button PA7 pressed
				DB_ON												// Enable debounce timer
			if (GPIOA->IDR & GPIO_IDR_IDR_7 && TIM5->CNT > 200) {	// Encoder button released - check enough time has elapsed to ensure not a bounce. A quick press if around 300, a long one around 8000+
				encoderBtnR = true;
				DB_OFF												// Disable debounce timer
			}
		} else {
			// read encoder pins using state table to debounce
			uint8_t pinState = (!(GPIOE->IDR & GPIO_IDR_IDR_8) << 1) | !(GPIOE->IDR & GPIO_IDR_IDR_9);		// Store position of each pin
			encoderStateR = encTable[encoderStateR & 0xF][pinState];	// Determine new state from the pins and state table.
			if (encoderStateR & 0x30) {
				encoderPendingR = (encoderStateR & 0x30) == DIR_CW ? 1 : -1;
			}
		}

		EXTI->PR |= EXTI_PR_PR7 | EXTI_PR_PR8 | EXTI_PR_PR9;		// Clear interrupt pending
	}

	// Left Encoder
	void EXTI15_10_IRQHandler(void) {

		uint8_t pinState = (!(GPIOE->IDR & GPIO_IDR_IDR_10) << 1) | !(GPIOE->IDR & GPIO_IDR_IDR_11);	// Store position of each pin
		encoderStateL = encTable[encoderStateL & 0xF][pinState];	// Determine new state from the pins and state table.
		if (encoderStateL & 0x30) {
			encoderPendingL = (encoderStateL & 0x30) == DIR_CW ? 1 : -1;
		}

		EXTI->PR |= EXTI_PR_PR10 | EXTI_PR_PR11;					// Clear interrupt pending
	}

	//	Coverage timer
	void TIM4_IRQHandler(void) {
		TIM4->SR &= ~TIM_SR_UIF;									// clear UIF flag
		coverageTimer ++;
	}

	// MIDI Decoder
	void UART4_IRQHandler(void) {
		if (UART4->SR | USART_SR_RXNE) {
			midi.MIDIQueue.push(UART4->DR);							// accessing DR automatically resets the receive flag
		}
	}
}



int main(void) {
	SystemClock_Config();					// Configure the clock and PLL - NB Currently done in SystemInit but will need updating for production board

	//	SystemInit();							// Activates floating point coprocessor and resets clock
	SystemCoreClockUpdate();				// Update SystemCoreClock (system clock frequency) derived from settings of oscillators, prescalers and PLL
	InitCoverageTimer();					// Timer 4 only activated/deactivated when CP_ON/CP_CAP macros are used
	InitDebounceTimer();					// Timer 5 used to count button press bounces
	InitLCDHardware();
	InitADC();
	InitEncoders();
	InitUART();

	lcd.Init();								// Initialize ILI9341 LCD
	InitSampleAcquisition();
	ui.ResetMode();

	// The FFT draw buffers are declared here and passed to the FFT Class as points to keep the size of the executable down
	uint16_t FFTDrawBuff0[(DRAWHEIGHT + 1) * FFTDRAWBUFFERWIDTH];
	uint16_t FFTDrawBuff1[(DRAWHEIGHT + 1) * FFTDRAWBUFFERWIDTH];
	fft.setDrawBuffer(FFTDrawBuff0, FFTDrawBuff1);

	CalibZeroPos = CalcZeroSize();

	while (1) {

		ui.handleEncoders();

		if (ui.menuMode) {

		} else if (displayMode == MIDI) {

			midi.ProcessMidi();

		} else if (displayMode == Fourier || displayMode == Waterfall) {

			fft.sampleCapture(false);									// checks if ready to start new capture

			if (fft.dataAvailable[0] || fft.dataAvailable[1]) {

				drawBufferNumber = fft.dataAvailable[0] ? 0 : 1;		// select correct draw buffer based on whether buffer 0 or 1 contains data
				if (displayMode == Fourier) {
					fft.runFFT(fft.FFTBuffer[drawBufferNumber]);
				}
				else
					fft.waterfall(fft.FFTBuffer[drawBufferNumber]);
			}

		} else if (displayMode == Circular) {


			if ((!circDrawing[0] && circDataAvailable[0] && (circDrawPos[1] >= zeroCrossings[1] || !circDrawing[1])) ||
				(!circDrawing[1] && circDataAvailable[1] && (circDrawPos[0] >= zeroCrossings[0] || !circDrawing[0]))) {								// check if we should start drawing

				drawBufferNumber = (!circDrawing[0] && circDataAvailable[0] && (circDrawPos[1] == zeroCrossings[1] || !circDrawing[1])) ? 0 : 1;
				circDrawing[drawBufferNumber] = true;
				circDrawPos[drawBufferNumber] = 0;
				lcd.DrawString(140, DRAWHEIGHT + 8, ui.floatToString(captureFreq[drawBufferNumber], true) + "Hz  ", &lcd.Font_Small, LCD_WHITE, LCD_BLACK);
			}

			// to have a continuous display drawing next sample as old sample is finishing
			for (drawBufferNumber = 0; drawBufferNumber < 2; drawBufferNumber++) {
				if (circDrawing[drawBufferNumber]) {

					// draw a line getting fainter as it goes from current position backwards
					for (int pos = std::max((int)circDrawPos[drawBufferNumber] - CIRCLENGTH, 0); pos <= circDrawPos[drawBufferNumber] && pos <= zeroCrossings[drawBufferNumber]; pos++) {

						int slopeOffset = 0;			// make negative to slope top left to bottom right and positive for opposite slope
						int b = (int)std::round(pos * LUTSIZE / zeroCrossings[drawBufferNumber] + LUTSIZE / 4 + slopeOffset) % LUTSIZE;
						int x = fft.SineLUT[b] * 70 + 160;


						int pixelA = CalcVertOffset(OscBufferA[drawBufferNumber][pos]);
						if (pos == std::max((int)circDrawPos[drawBufferNumber] - CIRCLENGTH, 0)) {
							prevPixelA = pixelA;
						}
						uint16_t greenShade = (63 - (circDrawPos[drawBufferNumber] - pos) / 3) << 5;
						uint16_t blueShade = (31 - (circDrawPos[drawBufferNumber] - pos) / 6);

						if (pos < (int)circDrawPos[drawBufferNumber] - CIRCLENGTH + 2) {
							greenShade = LCD_BLACK;
							blueShade = LCD_BLACK;
						}

						// Draw 'circle'
						lcd.DrawLine(x, pixelA, x, prevPixelA, greenShade);

						// Draw normal osc
						uint16_t oscPos = pos * DRAWWIDTH / zeroCrossings[drawBufferNumber];
						lcd.DrawLine(oscPos, pixelA, oscPos, prevPixelA, blueShade);

						prevPixelA = pixelA;
					}

					circDrawPos[drawBufferNumber] ++;
					if (circDrawPos[drawBufferNumber] == zeroCrossings[drawBufferNumber] + CIRCLENGTH){
						circDrawing[drawBufferNumber] = false;
						circDataAvailable[drawBufferNumber] = false;
					}
				}

			}

		} else if (displayMode == Oscilloscope) {

			if (!drawing && capturing) {								// check if we should start drawing
				drawBufferNumber = captureBufferNumber;
				drawing = true;
				drawPos = 0;
			}

			// Check if drawing and that the sample capture is at or ahead of the draw position
			if (drawing && (drawBufferNumber != captureBufferNumber || capturedSamples[captureBufferNumber] >= drawPos)) {

				// Draw a black line over previous sample - except at beginning and end where we shouldn't clear the voltage and frequency markers
				lcd.ColourFill(drawPos, (drawPos < 27 || drawPos > 250 ? 11 : 0), drawPos, DRAWHEIGHT - (drawPos < 27 ? 10: 0), LCD_BLACK);

				// Calculate offset between capture and drawing positions to display correct sample
				uint16_t calculatedOffset = (drawOffset[drawBufferNumber] + drawPos) % DRAWWIDTH;

				uint16_t pixelA = CalcVertOffset(OscBufferA[drawBufferNumber][calculatedOffset]);
				uint16_t pixelB = CalcVertOffset(OscBufferB[drawBufferNumber][calculatedOffset]);
				uint16_t pixelC = CalcVertOffset(OscBufferC[drawBufferNumber][calculatedOffset]);

				// Starting a new screen: Set previous pixel to current pixel and clear frequency calculations
				if (drawPos == 0) {
					prevPixelA = pixelA;
					prevPixelB = pixelB;
					prevPixelC = pixelC;
					freqBelowZero = false;
					freqCrossZero = 0;
				}

				//	frequency calculation - detect upwards zero crossings
				if (!freqBelowZero && OscBufferA[drawBufferNumber][calculatedOffset] < CalibZeroPos) {		// first time reading goes below zero
					freqBelowZero = true;
				}
				if (freqBelowZero && OscBufferA[drawBufferNumber][calculatedOffset] >= CalibZeroPos) {		// zero crossing
					//	second zero crossing - calculate frequency averaged over a number passes to smooth
					if (freqCrossZero > 0) {
						osc.Freq = (3 * osc.Freq + FreqFromPos(drawPos - freqCrossZero)) / 4;
					}
					freqCrossZero = drawPos;
					freqBelowZero = false;
				}


				// draw center line and voltage markers
				if (drawPos % 4 == 0) {
					lcd.DrawPixel(drawPos, DRAWHEIGHT / 2, LCD_GREY);
				}
				if (drawPos < 5) {
					for (int m = 0; m < voltScale * 2; ++m) {
						lcd.DrawPixel(drawPos, m * DRAWHEIGHT / (voltScale * 2), LCD_GREY);
					}
				}


				// Draw current samples as lines from previous pixel position to current sample position
				lcd.DrawLine(drawPos, pixelA, drawPos, prevPixelA, LCD_GREEN);
				lcd.DrawLine(drawPos, pixelB, drawPos, prevPixelB, LCD_LIGHTBLUE);
				lcd.DrawLine(drawPos, pixelC, drawPos, prevPixelC, LCD_ORANGE);

				// Store previous sample so next sample can be drawn as a line from old to new
				prevPixelA = pixelA;
				prevPixelB = pixelB;
				prevPixelC = pixelC;

				drawPos ++;
				if (drawPos == DRAWWIDTH){
					drawing = false;
				}

				// Draw trigger as a yellow cross
				if (drawPos == osc.TriggerX + 4) {
					uint16_t vo = CalcVertOffset(osc.TriggerY);
					if (vo > 4 && vo < DRAWHEIGHT - 4) {
						lcd.DrawLine(osc.TriggerX, vo - 4, osc.TriggerX, vo + 4, LCD_YELLOW);
						lcd.DrawLine(std::max(osc.TriggerX - 4, 0), vo, osc.TriggerX + 4, vo, LCD_YELLOW);
					}
				}


				if (drawPos == 1) {
					// Write voltage
					lcd.DrawString(0, 1, " " + ui.intToString(voltScale) + "v ", &lcd.Font_Small, LCD_GREY, LCD_BLACK);
					lcd.DrawString(0, DRAWHEIGHT - 10, "-" + ui.intToString(voltScale) + "v ", &lcd.Font_Small, LCD_GREY, LCD_BLACK);

					// Write frequency
					lcd.DrawString(250, 1, ui.floatToString(osc.Freq, false) + "Hz    ", &lcd.Font_Small, LCD_WHITE, LCD_BLACK);
				}
				CP_CAP
			}
		}
	}
}


