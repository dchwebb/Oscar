#include "stm32f4xx.h"
#include <cmath>
#include "initialisation.h"
#include "lcd.h"

#define OSCWIDTH 320
#define LUTSIZE 1024
#define FFTSAMPLES 1024
#define FFTDRAWBUFFERSIZE 160
#define FFTDRAWAFTERCALC true

extern uint32_t SystemCoreClock;
extern volatile uint16_t ADC_array[ADC_BUFFER_LENGTH];
float SineLUT[LUTSIZE];
volatile uint16_t OscBufferA[2][OSCWIDTH], OscBufferB[2][OSCWIDTH];
volatile uint16_t prevAPixel = 0, prevBPixel = 0, adcA, oldAdcA, adcB, capturePos = 0, drawPos = 0, bufferSamples = 0;
volatile bool capturing = false, drawing = false;
volatile uint8_t VertOffsetA = 30, VertOffsetB = 30, captureBufferNumber = 0, drawBufferNumber = 0;
volatile int16_t drawOffset[2] {0, 0};
volatile bool dataAvailable[2] {false, false};
volatile uint16_t capturedSamples[2] {0, 0};
volatile bool Encoder1Btn = false, oscFree = false, FFTMode = false;

volatile float FFTBuffer[2][FFTSAMPLES], candCos[FFTSAMPLES];
volatile uint16_t FFTPrevDraw[FFTSAMPLES];
 uint16_t FFTDrawBuffer[2][240 * FFTDRAWBUFFERSIZE];
constexpr int FFTbits = log2(FFTSAMPLES);
constexpr float FFTWidth = (FFTSAMPLES / 2) > OSCWIDTH ? 1 : (float)OSCWIDTH / (FFTSAMPLES / 2);

volatile uint32_t debugCount = 0, coverageTimer = 0, coverageTotal = 0;
volatile int16_t oldencoderUp = 0, oldencoderDown = 0, encoderUp = 0, encoderDown = 0, encoderVal = 0, encoderState = 0;

Lcd lcd;



struct  {
	uint16_t x = 10;
	uint16_t y = 125;
} trigger;


//	Interrupts: Use extern C to allow linker to find ISR
extern "C"
{
	void TIM3_IRQHandler(void) {

		TIM3->SR &= ~TIM_SR_UIF;					// clear UIF flag

		if (FFTMode) {
			if (capturePos == FFTSAMPLES) {
				dataAvailable[captureBufferNumber] = true;
				capturing = false;
			}

			if (capturing) {
				// For FFT Mode we want a value between +- 2047
				FFTBuffer[captureBufferNumber][capturePos] = 2047 - ((float)(ADC_array[0] + ADC_array[2] + ADC_array[4] + ADC_array[6]) / 4);
				capturePos ++;
			}


		} else {
			// Average the last four ADC readings to smooth noise
			adcA = (((float)(ADC_array[0] + ADC_array[2] + ADC_array[4] + ADC_array[6]) / 4) / 4096 * 240) - VertOffsetA;
			adcB = (((float)(ADC_array[1] + ADC_array[3] + ADC_array[5] + ADC_array[7]) / 4) / 4096 * 240) - VertOffsetB;

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
					if (drawOffset[captureBufferNumber] < 0)	drawOffset[captureBufferNumber] += OSCWIDTH;

					capturedSamples[captureBufferNumber] = trigger.x - 1;	// used to check if a sample is ready to be drawn
				}
			}

			// if capturing check if write buffer is full and switch to next buffer if so; if not full store current reading
			if (capturing && capturedSamples[captureBufferNumber] == OSCWIDTH - 1) {
				captureBufferNumber = captureBufferNumber == 1 ? 0 : 1;		// switch the capture buffer
				bufferSamples = 0;			// stores number of samples captured since switching buffers to ensure triggered mode works correctly
				capturing = false;
			}

			// If capturing or buffering samples waiting for trigger store current readings in buffer and increment counters
			if (capturing || !drawing || captureBufferNumber != drawBufferNumber) {
				OscBufferA[captureBufferNumber][capturePos] = adcA;
				OscBufferB[captureBufferNumber][capturePos] = adcB;
				oldAdcA = adcA;

				if (capturePos == OSCWIDTH - 1)		capturePos = 0;
				else								capturePos++;

				if (capturing)	capturedSamples[captureBufferNumber]++;
				else 			bufferSamples++;

			}


		}
	}

	// Encoder button
	void EXTI9_5_IRQHandler(void) {
		if (!(GPIOA->IDR & GPIO_IDR_IDR_7))				// Read Encoder button PA7
			Encoder1Btn = true;

		if (!(GPIOE->IDR & GPIO_IDR_IDR_8))				// Read Encoder up PE8
			encoderUp++;
		if (!(GPIOE->IDR & GPIO_IDR_IDR_9))				// Read Encoder down PE9
			encoderDown++;

		if (encoderUp > 1 || encoderDown > 1) {
			encoderVal += (encoderUp > encoderDown) ? 1 : -1;
			encoderUp = 0;
			encoderDown = 0;
		}

		EXTI->PR |= EXTI_PR_PR7 | EXTI_PR_PR8 | EXTI_PR_PR9;	// Clear interrupt pending
	}

	//	Coverage timer
	void TIM4_IRQHandler(void) {
		TIM4->SR &= ~TIM_SR_UIF;						// clear UIF flag
		coverageTimer ++;
	}
}

// Generate Sine LUT
void GenerateLUT(void) {
	for (int s = 0; s < LUTSIZE; s++){
		SineLUT[s] = sin(s * 2.0f * M_PI / LUTSIZE);
	}
}

void ResetSampleAcquisition() {
	TIM3->CR1 &= ~TIM_CR1_CEN;			// Disable the sample acquisiton timer
	lcd.ScreenFill(LCD_BLACK);
	capturing = drawing = false;
	bufferSamples = capturePos = oldAdcA = 0;
	TIM3->CR1 |= TIM_CR1_CEN;			// Reenable the sample acquisiton timer
}

// Fast fourier transform
void FFT(volatile float candSin[]) {

	CP_ON

	int bitReverse = 0;

	/*
	// create an test array to transform
	for (int i = 0; i < FFTSAMPLES; i++) {
		// Sine Wave + harmonic
		candSin[i] = 4096 * (sin(2.0f * M_PI * i / FFTSAMPLES) + (1.0f / harm) * sin(harm * 2.0f * M_PI * i / FFTSAMPLES));
		//candSin[i] = 4096 * ((2.0f * (FFTSAMPLES - i) / FFTSAMPLES) - 1);	// Saw Tooth
		//candSin[i] = i < (FFTSAMPLES / 2) ? 2047 : -2047;					// Square wave
	}*/

	// Bit reverse samples
	for (int i = 0; i < FFTSAMPLES; i++) {
		// assembly bit reverses i and then rotates right to correct bit length
		asm("rbit %[result], %[value]\n\t"
			"ror %[result], %[shift]"
			: [result] "=r" (bitReverse) : [value] "r" (i), [shift] "r" (32 - FFTbits));

		if (bitReverse > i) {
			// bit reverse samples
			float temp = candSin[i];
			candSin[i] = candSin[bitReverse];
			candSin[bitReverse] = temp;
		}
	}


	// Step through each column in the butterfly diagram
	int node = 1;
	while (node < FFTSAMPLES) {

		if (node == 1) {

			// for the first loop the sine and cosine values will be 1 and 0 in all cases, simplifying the logic
			for (int p1 = 0; p1 < FFTSAMPLES; p1 += 2) {
				int p2 = p1 + node;

				float sinP2 = candSin[p2];

				candSin[p2] = candSin[p1] - sinP2;
				candCos[p2] = 0;
				candSin[p1] = candSin[p1] + sinP2;
				candCos[p1] = 0;
			}
		} else if (node == FFTSAMPLES / 2) {

			// last node - this draws samples rather than calculate them
			for (uint16_t p1 = 1; p1 <= OSCWIDTH; p1++) {
				// Use Sine LUT to generate sine and cosine values faster than sine or cosine functions
				uint16_t b = std::round(p1 * LUTSIZE / (2 * node));
				float s = SineLUT[b];
				float c = SineLUT[b + LUTSIZE / 4 % LUTSIZE];

				int p2 = p1 + node;

				// true if drawing after FFT calculations
				if (FFTDRAWAFTERCALC) {
					candSin[p1] += c * candSin[p2] - s * candCos[p2];
					candCos[p1] += c * candCos[p2] + s * candSin[p2];
				} else {

					// Combine final node calculation sine and cosines to get amplitudes and store in alternate buffers, transmitting as each buffer is completed
					float hypotenuse = std::sqrt(std::pow(candSin[p1] + (c * candSin[p2] - s * candCos[p2]), 2) + std::pow(candCos[p1] + (c * candCos[p2] + s * candSin[p2]), 2));
					uint16_t top = std::min(239 * (1 - (hypotenuse / (512 * FFTSAMPLES))), 239.0f);

					uint8_t FFTDrawBufferNumber = (((p1 - 1) / FFTDRAWBUFFERSIZE) % 2 == 0) ? 0 : 1;		// Alternate between buffer 0 and buffer 1

					// draw column into memory buffer
					for (int h = 0; h < 240; ++h) {
						uint16_t buffPos = h * FFTDRAWBUFFERSIZE + ((p1 - 1) % FFTDRAWBUFFERSIZE);
						FFTDrawBuffer[FFTDrawBufferNumber][buffPos] = (h < top || p1 >= node) ? LCD_BLACK: LCD_BLUE;
					}

					// check if ready to draw next buffer
					if ((p1 % FFTDRAWBUFFERSIZE) == 0) {
						lcd.PatternFill(p1 - FFTDRAWBUFFERSIZE, 0, p1 - 1, 239, FFTDrawBuffer[FFTDrawBufferNumber]);
					}
				}
			}

		} else {
			// Step through each value of the W function
			for (int Wx = 0; Wx < node; Wx++) {
				// Use Sine LUT to generate sine and cosine values faster than sine or cosine functions
				int b = std::round(Wx * LUTSIZE / (2 * node));
				float s = SineLUT[b];
				float c = SineLUT[b + LUTSIZE / 4 % LUTSIZE];

				// replace pairs of nodes with updated values
				for (int p1 = Wx; p1 < FFTSAMPLES; p1 += node * 2) {
					int p2 = p1 + node;

					float sinP1 = candSin[p1];
					float cosP1 = candCos[p1];
					float sinP2 = candSin[p2];
					float cosP2 = candCos[p2];

					float t1 = c * sinP2 - s * cosP2;
					float t2 = c * cosP2 + s * sinP2;

					candSin[p2] = sinP1 - t1;
					candCos[p2] = cosP1 - t2;
					candSin[p1] = sinP1 + t1;
					candCos[p1] = cosP1 + t2;
				}
			}
		}

		node = node * 2;
	}

	if (FFTDRAWAFTERCALC) {
		// Combine sine and cosines to get amplitudes and store in alternate buffers, transmitting as each buffer is completed
		for (uint16_t i = 1; i <= std::min(FFTSAMPLES / 2, OSCWIDTH); i++) {

			float hypotenuse = std::sqrt(std::pow(candSin[i], 2) + std::pow(candCos[i], 2));
			uint16_t top = std::min(239 * (1 - (hypotenuse / (512 * FFTSAMPLES))), 239.0f);

			uint8_t FFTDrawBufferNumber = (((i - 1) / FFTDRAWBUFFERSIZE) % 2 == 0) ? 0 : 1;

			// draw column into memory buffer
			for (int h = 0; h < 240; ++h) {
				uint16_t buffPos = h * FFTDRAWBUFFERSIZE + ((i - 1) % FFTDRAWBUFFERSIZE);
				FFTDrawBuffer[FFTDrawBufferNumber][buffPos] = h < top ? LCD_BLACK: LCD_BLUE;

				if (top < 10 && i > 100) {
					int susp = 1;
				}
			}

			// check if ready to draw next buffer
			if ((i % FFTDRAWBUFFERSIZE) == 0) {
				debugCount = DMA2_Stream6->NDTR;
				lcd.PatternFill(i - FFTDRAWBUFFERSIZE, 0, i - 1, 239, FFTDrawBuffer[FFTDrawBufferNumber]);
			}
		}
	}

	CP_CAP
}


int main(void) {
	SystemInit();				// Activates floating point coprocessor and resets clock
//	SystemClock_Config();		// Configure the clock and PLL - NB Currently done in SystemInit but will need updating for production board
	SystemCoreClockUpdate();	// Update SystemCoreClock (system clock frequency) derived from settings of oscillators, prescalers and PLL
	InitCoverageTimer();		// Timer 4 only activated/deactivated when CP_ON/CP_CAP macros are used
	InitLCDHardware();
	InitADC();
	InitEncoders();
	GenerateLUT();				// Generate Sine LUT used for FFT
	lcd.Init();					// Initialize ILI9341 LCD
	lcd.Rotate(LCD_Landscape_Flipped);
	lcd.ScreenFill(LCD_BLACK);
	InitSampleAcquisition();

	//lcd.DrawString(60, 150, "Hello", &lcd.Font_Small, LCD_WHITE, LCD_BLUE);


	while (1) {

		if (encoderVal != 0) {
			if ((encoderVal * 2) + TIM3->ARR > 0)
				TIM3->ARR += encoderVal * 2;
			encoderVal = 0;
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

				FFT(FFTBuffer[drawBufferNumber]);
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
				lcd.ColourFill(drawPos, 0, drawPos, 239, LCD_BLACK);

				// Calculate offset between capture and drawing positions to display correct sample
				uint16_t calculatedOffset = (drawOffset[drawBufferNumber] + drawPos) % OSCWIDTH;

				// Set previous pixel to current pixel if starting a new screen
				if (drawPos == 0) {
					prevAPixel = OscBufferA[drawBufferNumber][calculatedOffset];
					prevBPixel = OscBufferB[drawBufferNumber][calculatedOffset];
				}

				// Draw current samples as lines from previous pixel position to current sample position
				lcd.DrawLine(drawPos, OscBufferA[drawBufferNumber][calculatedOffset], drawPos, prevAPixel, LCD_GREEN);
				lcd.DrawLine(drawPos, OscBufferB[drawBufferNumber][calculatedOffset], drawPos, prevBPixel, LCD_LIGHTBLUE);

				// Store previous sample so next sample can be drawn as a line from old to new
				prevAPixel = OscBufferA[drawBufferNumber][calculatedOffset];
				prevBPixel = OscBufferB[drawBufferNumber][calculatedOffset];

				if (drawPos == OSCWIDTH - 1) {
					int pause = 1;
				}

				drawPos ++;
				if (drawPos == OSCWIDTH){
					drawing = false;
				}

				// Draw trigger as a yellow cross
				if (drawPos == trigger.x + 4) {
					lcd.DrawLine(trigger.x, trigger.y - 4, trigger.x, trigger.y + 4, LCD_YELLOW);
					lcd.DrawLine(std::max(trigger.x - 4, 0), trigger.y, trigger.x + 4, trigger.y, LCD_YELLOW);
				}
				CP_CAP
			}
		}

	}
}
