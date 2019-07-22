#include "initialisation.h"
#include "ui.h"
#include "lcd.h"
#include "fft.h"
#include "midi.h"
#include "osc.h"


extern uint32_t SystemCoreClock;
volatile uint32_t SysTickVal = 0;

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

volatile uint16_t MIDIUnknown = 0;
volatile uint32_t MIDIDebug = 0;

//	default calibration values for 15k and 100k resistors on input opamp scaling to a maximum of 8v (slightly less for negative signals)
#if defined(STM32F722xx)
	volatile int16_t vCalibOffset = -4240;
	volatile float vCalibScale = 1.41f;
#elif defined(STM32F446xx)
	volatile int16_t vCalibOffset = -4990;
	volatile float vCalibScale = 1.41f;
#else
	volatile int16_t vCalibOffset = -4190;
	volatile float vCalibScale = 1.24f;
#endif

volatile int8_t voltScale = 8;
volatile uint16_t CalibZeroPos = 9985;

volatile uint16_t zeroCrossings[2] {0, 0};
volatile bool circDrawing[2] {false, false};
volatile uint16_t circDrawPos[2] {0, 0};
volatile uint16_t circPrevPixel[2] {0, 0};

volatile bool circDataAvailable[2] {false, false};
volatile float captureFreq[2] {0, 0};
volatile float circAngle;
mode displayMode = MIDI;
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

extern "C"
{
	#include "interrupts.h"
}

int main(void) {

	SystemInit();							// Activates floating point coprocessor and resets clock
	SystemClock_Config();					// Configure the clock and PLL - NB Currently done in SystemInit but will need updating for production board
	SystemCoreClockUpdate();				// Update SystemCoreClock (system clock frequency) derived from settings of oscillators, prescalers and PLL
	InitCoverageTimer();					// Timer 4 only activated/deactivated when CP_ON/CP_CAP macros are used
	InitDebounceTimer();					// Timer 5 used to count button press bounces
	InitLCDHardware();
	InitADC();
	InitEncoders();
	InitUART();
	InitSysTick();
	//	InitDAC();		// FIXME For testing

	lcd.Init();								// Initialize ILI9341 LCD

/*
	for (int p = 0; p < 50; ++p) {
		lcd.ColourFill(10, 2, 10, 6, LCD_RED);
		lcd.ColourFill(10, 4, 10, 8, LCD_BLUE);
		lcd.Delay(100000);
	}
*/


	InitSampleAcquisition();
	ui.ResetMode();

	// The FFT draw buffers are declared here and passed to the FFT Class as pointers to keep the size of the executable down
	uint16_t DrawBuff0[(DRAWHEIGHT + 1) * FFTDRAWBUFFERWIDTH];
	uint16_t DrawBuff1[(DRAWHEIGHT + 1) * FFTDRAWBUFFERWIDTH];
	fft.setDrawBuffer(DrawBuff0, DrawBuff1);
	osc.setDrawBuffer(DrawBuff0, DrawBuff1);


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
				CP_ON
			}

			// Check if drawing and that the sample capture is at or ahead of the draw position
			if (drawing && (drawBufferNumber != captureBufferNumber || capturedSamples[captureBufferNumber] >= drawPos)) {

				// Draw a black line over previous sample - except at beginning and end where we shouldn't clear the voltage and frequency markers
				//lcd.ColourFill(drawPos, (drawPos < 27 || drawPos > 250 ? 11 : 0), drawPos, DRAWHEIGHT - (drawPos < 27 ? 11 : 0), LCD_BLACK);

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

				// create draw buffer
				std::pair<uint16_t, uint16_t> AY = std::minmax(pixelA, (uint16_t)prevPixelA);
				std::pair<uint16_t, uint16_t> BY = std::minmax(pixelB, (uint16_t)prevPixelB);
				std::pair<uint16_t, uint16_t> CY = std::minmax(pixelC, (uint16_t)prevPixelC);

				uint8_t vOffset = (drawPos < 27 || drawPos > 250) ? 10 : 0;		// offset draw area so as not to overwrite voltage and freq labels
				for (uint8_t h = 0; h <= DRAWHEIGHT - (drawPos < 27 ? 12 : 0); ++h) {

					if (h < vOffset) {
						// do not draw
					} else if (osc.OscDisplay & 1 && h >= AY.first && h <= AY.second) {
						osc.DrawBuffer[osc.DrawBufferNumber][h - vOffset] = LCD_GREEN;
					} else if (osc.OscDisplay & 2 && h >= BY.first && h <= BY.second) {
						osc.DrawBuffer[osc.DrawBufferNumber][h - vOffset] = LCD_LIGHTBLUE;
					} else if (osc.OscDisplay & 4 && h >= CY.first && h <= CY.second) {
						osc.DrawBuffer[osc.DrawBufferNumber][h - vOffset] = LCD_ORANGE;
					} else if (drawPos % 4 == 0 && h == DRAWHEIGHT / 2) {
						osc.DrawBuffer[osc.DrawBufferNumber][h - vOffset] = LCD_GREY;
					} else {
						osc.DrawBuffer[osc.DrawBufferNumber][h - vOffset] = LCD_BLACK;
					}
				}
				if (drawPos < 5) {
					for (int m = 0; m < voltScale * 2; ++m) {
						osc.DrawBuffer[osc.DrawBufferNumber][m * DRAWHEIGHT / (voltScale * 2)] = LCD_GREY;
					}
				}

				//debugCount = DMA1_Stream5->NDTR;

				lcd.PatternFill(drawPos, vOffset, drawPos, DRAWHEIGHT - (drawPos < 27 ? 12 : 0), osc.DrawBuffer[osc.DrawBufferNumber]);
				osc.DrawBufferNumber = osc.DrawBufferNumber == 0 ? 1 : 0;

				// Store previous sample so next sample can be drawn as a line from old to new
				prevPixelA = pixelA;
				prevPixelB = pixelB;
				prevPixelC = pixelC;

				drawPos ++;
				if (drawPos == DRAWWIDTH){
					drawing = false;
					CP_CAP
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

			}

		}
	}
}
