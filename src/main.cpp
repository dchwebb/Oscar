#include "stm32f4xx.h"
#include "tm_stm32f4_ili9341.h"
#include "tm_stm32f4_fonts.h"
#include <stdio.h>
#include "initialisation.h"

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

	InitLCD();
	InitADC();

	//Initialize ILI9341
	TM_ILI9341_Init();
	TM_ILI9341_Rotate(TM_ILI9341_Orientation_Landscape_2);		// Rotate LCD 90 degrees
	TM_ILI9341_Fill(ILI9341_COLOR_BLACK);						// Fill lcd with black

	/*
	//Put string with black foreground color and blue background with 11x18px font
	TM_ILI9341_Puts(65, 130, "STM32F4 Discovery", &TM_Font_11x18, ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);
	//Put string with black foreground color and blue background with 11x18px font
	TM_ILI9341_Puts(60, 150, "ILI9341 LCD Module", &TM_Font_11x18, ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);
	//Put string with black foreground color and red background with 11x18px font
	TM_ILI9341_Puts(245, 225, "Mountjoy", &TM_Font_7x10, ILI9341_COLOR_BLACK, ILI9341_COLOR_ORANGE);

	DW_Pattern_Fill(50, 50);
*/

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
				TM_ILI9341_DrawLine(drawPos, 0, drawPos, 239, ILI9341_COLOR_BLACK);

				// Draw current samples as lines from previous pixel position to current sample position
				TM_ILI9341_DrawLine(drawPos, drawABuffer[calculatedOffset], drawPos, prevAPixel, ILI9341_COLOR_GREEN);
				TM_ILI9341_DrawLine(drawPos, drawBBuffer[calculatedOffset], drawPos, prevBPixel, ILI9341_COLOR_BLUE2);

				// Store previous sample so next sample can be drawn as a line from old to new
				prevAPixel = drawABuffer[calculatedOffset];
				prevBPixel = drawBBuffer[calculatedOffset];


				drawPos ++;
				if (drawPos == OSCWIDTH) drawing = false;

				// Draw trigger as a yellow cross
				if (drawPos == trigger.x + 4) {
					TM_ILI9341_DrawLine(trigger.x, trigger.y - 4, trigger.x, trigger.y + 4, ILI9341_COLOR_YELLOW);
					TM_ILI9341_DrawLine(trigger.x - 4, trigger.y, trigger.x + 4, trigger.y, ILI9341_COLOR_YELLOW);
				}

			}
		}


	}
}
