#include "stm32f4xx.h"
#include <cmath>
#include <string>
#include <sstream>
#include "initialisation.h"
#include "lcd.h"
#include "fft.h"



extern uint32_t SystemCoreClock;
extern volatile uint16_t ADC_array[ADC_BUFFER_LENGTH];
//volatile float candCos[FFTSAMPLES];
//uint16_t FFTDrawBuffer[2][(DRAWHEIGHT + 1) * FFTDRAWBUFFERSIZE];
//float SineLUT[LUTSIZE];

volatile uint16_t OscBufferA[2][DRAWWIDTH], OscBufferB[2][DRAWWIDTH];
volatile uint16_t prevPixelA = 0, prevPixelB = 0, adcA, oldAdcA, adcB, capturePos = 0, drawPos = 0, bufferSamples = 0;
volatile bool capturing = false, drawing = false;
volatile uint8_t VertOffsetA = 30, VertOffsetB = 30, captureBufferNumber = 0, drawBufferNumber = 0;
volatile int16_t drawOffset[2] {0, 0};
volatile bool dataAvailable[2] {false, false};
volatile uint16_t capturedSamples[2] {0, 0};
volatile bool Encoder1Btn = false, oscFree = false, FFTMode = true;
volatile int8_t encoderPendingL = 0, encoderPendingR = 0;
volatile uint16_t bounce = 0, nobounce = 0;
volatile uint32_t debugCount = 0, coverageTimer = 0, coverageTotal = 0;
volatile uint16_t maxHarm = 0;

#define ADC_BUFFER_LENGTH 8
volatile uint16_t ADC_array[ADC_BUFFER_LENGTH];


volatile int16_t oldencoderUp = 0, oldencoderDown = 0, encoderUp = 0, encoderDown = 0, encoderVal = 0, encoderState = 0;

Lcd lcd;
fft Fft;

struct  {
	uint16_t x = 10;
	uint16_t y = 9000;
} trigger;


//	Interrupts: Use extern C to allow linker to find ISR
extern "C"
{
	// Main sample capture
	void TIM3_IRQHandler(void) {

		TIM3->SR &= ~TIM_SR_UIF;					// clear UIF flag

		if (FFTMode) {
			if (capturePos == FFTSAMPLES) {
				dataAvailable[captureBufferNumber] = true;
				capturing = false;
			}

			if (capturing) {
				// For FFT Mode we want a value between +- 2047
				Fft.FFTBuffer[captureBufferNumber][capturePos] = 2047 - ((float)(ADC_array[0] + ADC_array[2] + ADC_array[4] + ADC_array[6]) / 4);
				capturePos ++;
			}


		} else {
			// Average the last four ADC readings to smooth noise
			adcA = ADC_array[0] + ADC_array[2] + ADC_array[4] + ADC_array[6];
			adcB = ADC_array[1] + ADC_array[3] + ADC_array[5] + ADC_array[7];

			// check if we should start capturing - ie not drawing from the capture buffer and crossed over the trigger threshold (or in free mode)
			if (!capturing && (!drawing || captureBufferNumber != drawBufferNumber) && (oscFree || (bufferSamples > trigger.x && oldAdcA < trigger.y && adcA >= trigger.y))) {
				capturing = true;

				if (oscFree) {										// free running mode
					capturePos = 0;
					drawOffset[captureBufferNumber] = 0;
					capturedSamples[captureBufferNumber] = -1;
				} else {
					// calculate the drawing offset based on the current capture position minus the horizontal trigger position
					drawOffset[captureBufferNumber] = capturePos - trigger.x;
					if (drawOffset[captureBufferNumber] < 0)	drawOffset[captureBufferNumber] += DRAWWIDTH;

					capturedSamples[captureBufferNumber] = trigger.x - 1;	// used to check if a sample is ready to be drawn
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
				oldAdcA = adcA;

				if (capturePos == DRAWWIDTH - 1)	capturePos = 0;
				else								capturePos++;

				if (capturing)	capturedSamples[captureBufferNumber]++;
				else 			bufferSamples++;

			}
		}
	}

	// Right Encoder
	void EXTI9_5_IRQHandler(void) {
		if (!(GPIOA->IDR & GPIO_IDR_IDR_7))				// Read Encoder button PA7
			Encoder1Btn = true;

		// Encoder sequence is one goes down then the other (and v bouncy) - set pending based on first action then let main loop check both are up before actioning
		if (!encoderPendingR && !(GPIOE->IDR & GPIO_IDR_IDR_8) && (GPIOE->IDR & GPIO_IDR_IDR_9))
			encoderPendingR = 1;

		if (!encoderPendingR && !(GPIOE->IDR & GPIO_IDR_IDR_9) && (GPIOE->IDR & GPIO_IDR_IDR_8))
			encoderPendingR = -1;


		EXTI->PR |= EXTI_PR_PR7 | EXTI_PR_PR8 | EXTI_PR_PR9;	// Clear interrupt pending
	}

	// Left Encoder
	void EXTI15_10_IRQHandler(void) {

		// Encoder sequence is one goes down then the other (and v bouncy) - set pending based on first action then let main loop check both are up before actioning
		if (!encoderPendingL && !(GPIOE->IDR & GPIO_IDR_IDR_10) && (GPIOE->IDR & GPIO_IDR_IDR_11))
			encoderPendingL = 1;

		if (!encoderPendingL && !(GPIOE->IDR & GPIO_IDR_IDR_11) && (GPIOE->IDR & GPIO_IDR_IDR_10))
			encoderPendingL = -1;

		EXTI->PR |= EXTI_PR_PR10 | EXTI_PR_PR11;			// Clear interrupt pending
	}

	//	Coverage timer
	void TIM4_IRQHandler(void) {
		TIM4->SR &= ~TIM_SR_UIF;							// clear UIF flag
		coverageTimer ++;
	}
}


void DrawUI() {
	// Draw UI
	lcd.ColourFill(0, DRAWHEIGHT + 1, DRAWWIDTH - 1, DRAWHEIGHT + 1, LCD_GREY);
	lcd.ColourFill(0, 239, DRAWWIDTH - 1, 239, LCD_GREY);
	lcd.ColourFill(0, DRAWHEIGHT + 1, 0, 239, LCD_GREY);
	lcd.ColourFill(90, DRAWHEIGHT + 1, 90, 239, LCD_GREY);
	lcd.ColourFill(230, DRAWHEIGHT + 1, 230, 239, LCD_GREY);
	lcd.ColourFill(319, DRAWHEIGHT + 1, 319, 239, LCD_GREY);

	lcd.DrawString(10, DRAWHEIGHT + 8, "Zoom Horiz", &lcd.Font_Small, LCD_GREY, LCD_BLACK);
	lcd.DrawString(240, DRAWHEIGHT + 8, "Zoom Vert", &lcd.Font_Small, LCD_GREY, LCD_BLACK);

	uint16_t screenMs = std::round(640000.0f * TIM3->PSC * TIM3->ARR / SystemCoreClock);
	std::stringstream ss;
	ss << screenMs << "ms    ";

	std::string s = ss.str();
	lcd.DrawString(140, DRAWHEIGHT + 8, s, &lcd.Font_Small, LCD_WHITE, LCD_BLACK);

}

void ResetSampleAcquisition() {
	TIM3->CR1 &= ~TIM_CR1_CEN;			// Disable the sample acquisiton timer
	lcd.ScreenFill(LCD_BLACK);
	DrawUI();
	capturing = drawing = false;
	bufferSamples = capturePos = oldAdcA = 0;
	TIM3->CR1 |= TIM_CR1_CEN;			// Reenable the sample acquisiton timer
}

inline uint16_t CalcVertOffset(volatile uint16_t& vPos, const uint16_t& vOffset) {
	return (((float)vPos / 4) / 4096 * DRAWHEIGHT) - vOffset;
}

int main(void) {
	SystemInit();				// Activates floating point coprocessor and resets clock
//	SystemClock_Config();		// Configure the clock and PLL - NB Currently done in SystemInit but will need updating for production board
	SystemCoreClockUpdate();	// Update SystemCoreClock (system clock frequency) derived from settings of oscillators, prescalers and PLL
	InitCoverageTimer();		// Timer 4 only activated/deactivated when CP_ON/CP_CAP macros are used
	InitLCDHardware();
	InitADC();
	InitEncoders();

	lcd.Init();					// Initialize ILI9341 LCD
	InitSampleAcquisition();
	DrawUI();

	while (1) {

		if (encoderPendingR && (GPIOE->IDR & GPIO_IDR_IDR_8) && (GPIOE->IDR & GPIO_IDR_IDR_9)) {
			TIM3->ARR += 4 * encoderPendingR;
			encoderPendingR = 0;
			DrawUI();
		}
		if (encoderPendingL && (GPIOE->IDR & GPIO_IDR_IDR_11) && (GPIOE->IDR & GPIO_IDR_IDR_10)) {
			TIM3->ARR += encoderPendingL;
			encoderPendingL = 0;
			DrawUI();
		}

		if (Encoder1Btn) {
			Encoder1Btn = false;
			FFTMode = !FFTMode;
			ResetSampleAcquisition();
		}

		// Fourier Transform
		if (FFTMode) {
			if (!capturing && (!dataAvailable[0] || !dataAvailable[1])) {
				capturing = true;
				capturePos = 0;
				captureBufferNumber = dataAvailable[0] ? 1 : 0;
			} else {
				// select correct draw buffer based on whether buffer 0 or 1 contains data
				if (dataAvailable[0])		drawBufferNumber = 0;
				else if (dataAvailable[1])	drawBufferNumber = 1;
				else continue;

				Fft.runFFT(Fft.FFTBuffer[drawBufferNumber]);
				dataAvailable[drawBufferNumber] = false;
			}


		} else {

			// Oscilloscope mode
			if (!drawing && capturing) {					// check if we should start drawing
				drawBufferNumber = captureBufferNumber;
				drawing = true;
				drawPos = 0;
			}

			// Check if drawing and that the sample capture is at or ahead of the draw position
			if (drawing && (drawBufferNumber != captureBufferNumber || capturedSamples[captureBufferNumber] >= drawPos)) {
				CP_ON
				// Draw a black line over previous sample
				lcd.ColourFill(drawPos, 0, drawPos, DRAWHEIGHT, LCD_BLACK);

				// Calculate offset between capture and drawing positions to display correct sample
				uint16_t calculatedOffset = (drawOffset[drawBufferNumber] + drawPos) % DRAWWIDTH;

				uint16_t pixelA = CalcVertOffset(OscBufferA[drawBufferNumber][calculatedOffset], VertOffsetA);
				uint16_t pixelB = CalcVertOffset(OscBufferB[drawBufferNumber][calculatedOffset], VertOffsetB);

				// Set previous pixel to current pixel if starting a new screen
				if (drawPos == 0) {
					prevPixelA = pixelA;
					prevPixelB = pixelB;
				}

				// Draw current samples as lines from previous pixel position to current sample position
				lcd.DrawLine(drawPos, pixelA, drawPos, prevPixelA, LCD_GREEN);
				lcd.DrawLine(drawPos, pixelB, drawPos, prevPixelB, LCD_LIGHTBLUE);

				// Store previous sample so next sample can be drawn as a line from old to new
				prevPixelA = pixelA;
				prevPixelB = pixelB;

				drawPos ++;
				if (drawPos == DRAWWIDTH){
					drawing = false;
				}

				// Draw trigger as a yellow cross
				if (drawPos == trigger.x + 4) {
					lcd.DrawLine(trigger.x, CalcVertOffset(trigger.y, VertOffsetA) - 4, trigger.x, CalcVertOffset(trigger.y, VertOffsetA) + 4, LCD_YELLOW);
					lcd.DrawLine(std::max(trigger.x - 4, 0), CalcVertOffset(trigger.y, VertOffsetA), trigger.x + 4, CalcVertOffset(trigger.y, VertOffsetA), LCD_YELLOW);
				}
				CP_CAP
			}
		}

	}
}


