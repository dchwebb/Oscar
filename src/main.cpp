#include "stm32f4xx.h"
#include <cmath>
#include "initialisation.h"
#include "lcd.h"

#define OSCWIDTH 320
#define LUTSIZE 1024
#define FFTSAMPLES 256

extern uint32_t SystemCoreClock;
extern volatile uint16_t ADC_array[ADC_BUFFER_LENGTH];

volatile int16_t adcA, oldAdcA, adcB;
volatile int16_t ChannelA0[OSCWIDTH];
volatile int16_t ChannelA1[OSCWIDTH];
volatile int16_t ChannelB0[OSCWIDTH];
volatile int16_t ChannelB1[OSCWIDTH];
volatile int16_t* captureABuffer;
volatile int16_t* captureBBuffer;
volatile int16_t* drawABuffer;
volatile int16_t* drawBBuffer;
volatile uint16_t capturePos = 0;
volatile uint16_t drawPos = 0;
volatile uint16_t prevAPixel = 0;
volatile uint16_t prevBPixel = 0;
volatile bool capturing = false;
volatile int16_t* captureSamples = 0;
volatile int16_t captureSamples0 = 0;
volatile int16_t captureSamples1 = 0;
volatile bool drawing = false;
volatile uint8_t captureBufferNumber = 0;
volatile uint8_t drawBufferNumber = 0;
int16_t drawOffset[2] {0, 0};
float SineLUT[LUTSIZE];
volatile bool oscFree = false, FFTMode = false;
volatile bool dataAvailable[2] {false, false};
volatile bool dataIn0 = false, dataIn1 = false;

uint8_t VertOffsetA = 30, VertOffsetB = 30;
volatile uint16_t bufferSamples = 0;

volatile uint16_t DrawTest[22];

volatile uint32_t coverageTimer = 0;
volatile uint32_t coverageTotal = 0;
float maxA = 0;

Lcd lcd;

struct  {
	uint16_t x = 0;
	uint16_t y = 125;
} trigger;


//	Interrupts: Use extern C to allow linker to find ISR
extern "C"
{
	void TIM3_IRQHandler(void) {

		TIM3->SR &= ~TIM_SR_UIF;					// clear UIF flag

		if (FFTMode) {
			// For FFT Mode we want a value between +- 2047
			adcA = 2047 - ((float)(ADC_array[0] + ADC_array[2] + ADC_array[4] + ADC_array[6]) / 4);

			if (capturePos == FFTSAMPLES) {
				dataAvailable[captureBufferNumber] = true;
				capturing = false;
			}

			/*if (!capturing) {
				uint8_t nextBuffer = captureBufferNumber == 0 ? 1 : 0;
				if (!dataAvailable[nextBuffer] && (!drawing || drawBufferNumber != nextBuffer)) {
					captureBufferNumber == nextBuffer;
					capturing = true;
				}

				if (capturing) {
					captureABuffer = captureBufferNumber == 0 ? ChannelA0 : ChannelA1;
					capturePos = 0;
				}
			}*/

			if (capturing) {
				captureABuffer[capturePos] = adcA;
				capturePos ++;
			}


		} else {
			// Average the last four ADC readings to smooth noise
			adcA = (((float)(ADC_array[0] + ADC_array[2] + ADC_array[4] + ADC_array[6]) / 4) / 4096 * 240) - VertOffsetA;
			adcB = (((float)(ADC_array[1] + ADC_array[3] + ADC_array[5] + ADC_array[7]) / 4) / 4096 * 240) - VertOffsetB;

			// check if we should start capturing - ie not drawing from the capture buffer and crossed over the trigger threshold (or in free mode)
			if (!capturing && (!drawing || captureBufferNumber != drawBufferNumber) && (oscFree || (bufferSamples > trigger.x && oldAdcA < trigger.y && adcA >= trigger.y))) {
				capturing = true;

				captureSamples = captureBufferNumber == 0 ? &captureSamples0 : &captureSamples1;	// holds the number of drawable samples in the current buffer

				if (oscFree) {
					// free running mode
					capturePos = 0;
					drawOffset[captureBufferNumber] = 0;
					*captureSamples = -1;
				} else {
					// calculate the drawing offset based on the current capture position minus the horizontal trigger position
					drawOffset[captureBufferNumber] = capturePos - trigger.x;
					if (drawOffset[captureBufferNumber] < 0)	drawOffset[captureBufferNumber] += OSCWIDTH;

					*captureSamples = trigger.x - 1;			// used to check if a sample is ready to be drawn
				}
			}

			// if capturing check if write buffer is full and switch to next buffer if so; if not full store current reading
			if (capturing && *captureSamples == OSCWIDTH - 1) {
				// switch the capture buffer and get a pointer to the current capture buffer
				captureBufferNumber = captureBufferNumber == 1 ? 0 : 1;
				captureABuffer = captureBufferNumber == 0 ? ChannelA0 : ChannelA1;
				captureBBuffer = captureBufferNumber == 0 ? ChannelB0 : ChannelB1;

				bufferSamples = 0;			// used to hold the number of samples captured since switching buffers to ensure triggered mode works correctly
				capturing = false;
			}

			captureABuffer[capturePos] = adcA;
			captureBBuffer[capturePos] = adcB;

			if (capturePos == OSCWIDTH - 1)		capturePos = 0;
			else								capturePos++;

			if (capturing)	(*captureSamples)++;
			else 			bufferSamples++;

			oldAdcA = adcA;
		}
	}

	//	Coverage timer
	void TIM4_IRQHandler(void) {
		TIM4->SR &= ~TIM_SR_UIF;					// clear UIF flag
		coverageTimer ++;
	}
}

// Generate Sine LUT
void GenerateLUT(void) {
	for (int s = 0; s < LUTSIZE; s++){
		SineLUT[s] = sin(s * 2.0f * M_PI / LUTSIZE);
	}
}

inline float QuickHypotenuse(float a, float b) {
	//	Algorithm to generate an approximate hypotenuse to a max error ≈ 1.04 %
	if (a > b) { float t = a; a = b;	b = t;	}
	if (b < 0.1) return a;
	return b + 0.428 * a * a / b;
}


//float candSin[FFTSAMPLES];
float candCos[FFTSAMPLES];

// Fast fourier transform
void FFT(volatile int16_t candSin[]) {

	constexpr int bits = log2(FFTSAMPLES);
	int br = 0;
/*

	// create an test array to transform
	for (int i = 0; i < FFTSAMPLES; i++) {
		// Sine Wave + harmonic
		candSin[i] = 4096 * (sin(2.0f * M_PI * i / FFTSAMPLES) + (1.0f / harm) * sin(harm * 2.0f * M_PI * i / FFTSAMPLES));

		//candSin[i] = 4096 * ((2.0f * (FFTSAMPLES - i) / FFTSAMPLES) - 1);	// Saw Tooth
		//candSin[i] = i < (FFTSAMPLES / 2) ? 2047 : -2047;		// Square wave
	}

*/
	//CP_ON

	// Bit reverse samples
	for (int i = 0; i < FFTSAMPLES; i++) {
		// assembly bit reverses i and then rotates right to correct bit length
		asm("rbit %[result], %[value]\n\t"
			"ror %[result], %[shift]"
			: [result] "=r" (br) : [value] "r" (i), [shift] "r" (32 - bits));

		if (br > i) {
			// bit reverse samples
			float temp = candSin[i];
			candSin[i] = candSin[br];
			candSin[br] = temp;
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
		} else {
			// Step through each value of the W function
			for (int Wx = 0; Wx < node; Wx++) {
				// Use Sine LUT to generate sine and cosine values faster
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

	// Combine sine and cosines to get amplitudes
	constexpr int width = OSCWIDTH / (FFTSAMPLES / 2);
	for (int i = 1; i <= FFTSAMPLES / 2; i++) {

		int left = (i - 1) * width;

		//float x = std::sqrt(std::pow(candSin[i], 2) + std::pow(candCos[i], 2));
		float x = QuickHypotenuse(candSin[i], candCos[i]);

		int top = 239 * (1 - (x / (2048 * FFTSAMPLES)));
		int x1 = left + width;

		lcd.ColourFill(left, top, x1, 239, LCD_BLUE);

	}
	//CP_CAP

}


int main(void) {
	SystemInit();				// Activates floating point coprocessor and resets clock
//	SystemClock_Config();		// Configure the clock and PLL - NB Currently done in SystemInit but will need updating for production board
	SystemCoreClockUpdate();	// Update SystemCoreClock (system clock frequency) derived from settings of oscillators, prescalers and PLL
	InitCoverageTimer();		// Timer 4 only activated/deactivated when CP_ON/CP_CAP macros are used
	InitLCDHardware();
	InitADC();
	GenerateLUT();				// Generate Sine LUT used for FFT
	lcd.Init();					// Initialize ILI9341 LCD
	lcd.Rotate(LCD_Landscape_Flipped);
	lcd.ScreenFill(LCD_BLACK);

	// Fourier Transform

	//while (1);
	/*
	while (1) {
		for (int fft = 2; fft < 64; fft++) {
			FFT(fft);
		}
		lcd.ScreenFill(LCD_BLACK);
	}*/

	// Test code
	//lcd.DrawString(60, 150, "Hello", &lcd.Font_Small, LCD_WHITE, LCD_BLUE);
	//lcd.DrawLine(0, 0, 120, 40, LCD_RED);

	captureABuffer = ChannelA0;
	captureBBuffer = ChannelB0;
	InitSampleAcquisition();

	while (1) {

		if (FFTMode) {
			if (!capturing && (!dataAvailable[0] || !dataAvailable[1])) {
				capturing = true;
				capturePos = 0;
				captureBufferNumber = dataAvailable[0] ? 1 : 0;
				captureABuffer = captureBufferNumber == 0 ? ChannelA0 : ChannelA1;
			} else {

				if (dataAvailable[0])		drawBufferNumber = 0;
				else if (dataAvailable[1])	drawBufferNumber = 1;
				else continue;

				lcd.ScreenFill(LCD_BLACK);

				FFT(drawBufferNumber == 0 ? ChannelA0 : ChannelA1);
				dataAvailable[drawBufferNumber] = false;
			}
		} else {
			// check if we should start drawing
			if (!drawing && capturing) {
				drawBufferNumber = captureBufferNumber;
				drawing = true;
				drawPos = 0;

				// Get a pointer to the current draw buffer
				drawABuffer = drawBufferNumber == 0 ? ChannelA0 : ChannelA1;
				drawBBuffer = drawBufferNumber == 0 ? ChannelB0 : ChannelB1;
			}

			// Check if drawing and that the sample capture is at or ahead of the draw position
			if (drawing && (drawBufferNumber != captureBufferNumber || (captureBufferNumber == 0 ? captureSamples0 : captureSamples1) >= drawPos)) {

				// Calculate offset between capture and drawing positions to display correct sample
				uint16_t calculatedOffset = (drawOffset[drawBufferNumber] + drawPos) % OSCWIDTH;

				// Set previous pixel to current pixel if starting a new screen
				if (drawPos == 0) {
					prevAPixel = drawABuffer[calculatedOffset];
					prevBPixel = drawBBuffer[calculatedOffset];
				}

				// Draw a black line over previous sample
				lcd.DrawLine(drawPos, 0, drawPos, 239, LCD_BLACK);

				// Draw current samples as lines from previous pixel position to current sample position
				lcd.DrawLine(drawPos, drawABuffer[calculatedOffset], drawPos, prevAPixel, LCD_GREEN);
				lcd.DrawLine(drawPos, drawBBuffer[calculatedOffset], drawPos, prevBPixel, LCD_LIGHTBLUE);

				// Store previous sample so next sample can be drawn as a line from old to new
				prevAPixel = drawABuffer[calculatedOffset];
				prevBPixel = drawBBuffer[calculatedOffset];

				drawPos ++;
				if (drawPos == OSCWIDTH) drawing = false;

				// Draw trigger as a yellow cross
				if (drawPos == trigger.x + 4) {
					lcd.DrawLine(trigger.x, trigger.y - 4, trigger.x, trigger.y + 4, LCD_YELLOW);
					lcd.DrawLine(std::max(trigger.x - 4, 0), trigger.y, trigger.x + 4, trigger.y, LCD_YELLOW);
				}
			}
		}

	}
}
