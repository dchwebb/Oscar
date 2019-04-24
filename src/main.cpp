#include "stm32f4xx.h"
//#include <stdio.h>
#include "initialisation.h"
#include "lcd.h"

#define OSCWIDTH 320

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

Lcd lcd;


struct  {
	uint16_t x = 10;
	uint16_t y = 125;
} trigger;

//	Use extern C to allow linker to find ISR
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

	// Test code
	lcd.DrawString(60, 150, "Hello", &lcd.Font_Small, LCD_WHITE, LCD_BLUE);
	lcd.DrawLine(0, 0, 120, 40, LCD_RED);

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
