#include "stm32f4xx.h"
#include <cmath>
#include "initialisation.h"
#include "lcd.h"

#define OSCWIDTH 320
#define LUTSIZE 1024

extern uint32_t SystemCoreClock;
extern volatile uint16_t ADC_array[ADC_BUFFER_LENGTH];

volatile uint16_t adc0, oldAdc0, adc1;
volatile uint16_t ChannelA0[OSCWIDTH];
volatile uint16_t ChannelA1[OSCWIDTH];
volatile uint16_t ChannelB0[OSCWIDTH];
volatile uint16_t ChannelB1[OSCWIDTH];
volatile uint16_t* captureABuffer;
volatile uint16_t* captureBBuffer;
volatile uint16_t* drawABuffer;
volatile uint16_t* drawBBuffer;
volatile uint16_t capturePos = 0;
volatile uint16_t drawPos = 0;
volatile uint16_t prevAPixel = 0;
volatile uint16_t prevBPixel = 0;
volatile bool capturing = false;
volatile int16_t captureSamples = 0;
volatile bool drawing = false;
volatile uint8_t captureBufferNumber = 0;
volatile uint8_t drawBufferNumber = 0;
int16_t drawOffset[2] {0, 0};
float SineLUT[LUTSIZE];

volatile uint32_t coverageTimer = 0;
volatile uint32_t coverageTotal = 0;

Lcd lcd;

struct  {
	uint16_t x = 10;
	uint16_t y = 125;
} trigger;


//	Interrupts: Use extern C to allow linker to find ISR
extern "C"
{
	void TIM3_IRQHandler(void) {
		if (TIM3->SR & TIM_SR_UIF) 						// if UIF flag is set
		{
			TIM3->SR &= ~TIM_SR_UIF;					// clear UIF flag

			adc0 = (((float)(ADC_array[0] + ADC_array[2] + ADC_array[4] + ADC_array[6]) / 4) / 4096 * 240) - 30;
			adc1 = (((float)(ADC_array[1] + ADC_array[3] + ADC_array[5] + ADC_array[7]) / 4) / 4096 * 240) - 30;
			//adc0 = ((float)ADC_array[0] / 4096 * 240) - 30;

			// check if we should start capturing - ie not drawing from the capture buffer and crossed over the trigger threshold
			if ((!drawing || captureBufferNumber != drawBufferNumber) && (!capturing && oldAdc0 < trigger.y && adc0 >= trigger.y)) {
				capturing = true;

				// calculate the drawing offset based on the current capture position minus the horizontal trigger position
				drawOffset[captureBufferNumber] = capturePos - trigger.x;
				if (drawOffset[captureBufferNumber] < 0) drawOffset[captureBufferNumber] += OSCWIDTH;

				// captureSamples is used to check if a sample is ready to be drawn
				captureSamples = trigger.x - 1;
			}

			// if capturing check if write buffer is full and switch to next buffer if so; if not full store current reading
			if (capturing && capturePos == drawOffset[captureBufferNumber]) {
				capturing = false;

				// switch the capture buffer and get a pointer to the current capture buffer
				captureBufferNumber = captureBufferNumber == 1 ? 0 : 1;
				captureABuffer = captureBufferNumber == 0 ? ChannelA0 : ChannelA1;
				captureBBuffer = captureBufferNumber == 0 ? ChannelB0 : ChannelB1;
			}
			captureABuffer[capturePos] = adc0;
			captureBBuffer[capturePos] = adc1;

			if (capturePos == OSCWIDTH - 1) {
				capturePos = 0;
			} else {
				capturePos++;
			}

			if (capturing) captureSamples++;

			oldAdc0 = adc0;

		}
	}

	//	Coverage timer
	void TIM4_IRQHandler(void) {
		if (TIM4->SR & TIM_SR_UIF) 						// if UIF flag is set
		{
			TIM4->SR &= ~TIM_SR_UIF;					// clear UIF flag
			coverageTimer ++;
		}
	}
}

// Generate Sine LUT
void GenerateLUT(void) {
	for (int s = 0; s < LUTSIZE; s++){
		SineLUT[s] = sin(s * 2.0f * M_PI / LUTSIZE);
	}
}

#define samples 128
float candSin[samples];
float candCos[samples];

// Fast fourier transform
void FFT() {

	int bits = log2(samples);
	int br = 0;
	coverageTimer = 0;

	// create an test array to transform
	for (int i = 0; i < samples; i++) {
		// Saw Tooth
		//candSin[i] = (2.0f * (samples - i) / samples) - 1;

		// Square wave
		candSin[i] = i < (samples / 2) ? 1 : 0;

		candCos[i] = 0;
	}
	TIM4->CR1 &= ~TIM_CR1_CEN;
	// Bit reverse samples
	for (int i = 0; i < samples; i++) {
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

	// Carry out FFT traversing butterfly diagram
	int node = 1;
	// Step through each column in the butterfly diagram
	while (node < samples) {

		// Step through each value of the W function
		for (int Wx = 0; Wx < node; Wx++) {
			float a = Wx * M_PI / node;
			float c = cos(a);
			float s = sin(a);

			// replace pairs of nodes with updated values
			for (int p1 = Wx; p1 < samples; p1 += node * 2) {
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

				/*candSin[p1] = sinP1;
				candCos[p1] = cosP1;
				candSin[p2] = sinP2;
				candCos[p2] = cosP2;*/
			}
		}

		node = node * 2;
	}


	// Combine sine and cosines to get amplitudes
	int width = OSCWIDTH / (samples / 2);
	for (int i = 1; i <= samples / 2; i++) {

		int left = (i - 1) * width;
		float x = std::sqrt(std::pow(candSin[i], 2) + std::pow(candCos[i], 2));
		int top = 239 * (1 - (x / (samples / 2)));
		int x1 = left + width;

		lcd.ColourFill(left, top, x1, 239, LCD_BLUE);

	}

}

int main(void) {
	SystemInit();				// Activates floating point coprocessor and resets clock
//	SystemClock_Config();		// Configure the clock and PLL - NB Currently done in SystemInit but will need updating for production board
	SystemCoreClockUpdate();	// Update SystemCoreClock (system clock frequency) derived from settings of oscillators, prescalers and PLL

	InitLCDHardware();
	InitADC();

	//Initialize ILI9341 LCD
	lcd.Init();
	lcd.Rotate(LCD_Landscape_Flipped);
	lcd.ScreenFill(LCD_BLACK);


	InitCoverageTimer();

	// Slow Fourier Transform
	FFT();

	TIM4->CR1 &= ~TIM_CR1_CEN;
	coverageTotal = (coverageTimer * 65536) + TIM4->CNT;


	//coverage65k = coverageTimer;
	//coverageCnt = TIM4->CNT;

	// Test code
	//lcd.DrawString(60, 150, "Hello", &lcd.Font_Small, LCD_WHITE, LCD_BLUE);
	//lcd.DrawLine(0, 0, 120, 40, LCD_RED);

	captureABuffer = ChannelA0;
	captureBBuffer = ChannelB0;
	InitTimer();

	while (1) {

		// check if we should start drawing
		if (!drawing && capturing) {
			drawing = true;
			drawPos = 0;
			drawBufferNumber = captureBufferNumber;

			// Get a pointer to the current draw buffer
			drawABuffer = drawBufferNumber == 0 ? ChannelA0 : ChannelA1;
			drawBBuffer = drawBufferNumber == 0 ? ChannelB0 : ChannelB1;
		}


		if (drawing) {

			// Check that the sample has been captured
			if (drawBufferNumber != captureBufferNumber || captureSamples >= drawPos) {

				// Calculate offset between capture and drawing positions to display correct sample
				uint16_t calculatedOffset = drawOffset[drawBufferNumber] + drawPos;
				if (calculatedOffset >= OSCWIDTH) calculatedOffset = calculatedOffset - OSCWIDTH;

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
					lcd.DrawLine(trigger.x - 4, trigger.y, trigger.x + 4, trigger.y, LCD_YELLOW);
				}

			}
		}


	}
}
