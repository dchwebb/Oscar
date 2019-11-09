#include "initialisation.h"
#include "config.h"
#include "ui.h"
#include "lcd.h"
#include "fft.h"
#include "midi.h"
#include "osc.h"


extern uint32_t SystemCoreClock;
volatile uint32_t SysTickVal = 0;

//	default calibration values for 15k and 100k resistors on input opamp scaling to a maximum of 8v (slightly less for negative signals)
#if defined(STM32F722xx) || defined(STM32F446xx)
	volatile int16_t vCalibOffset = -4240;
	volatile float vCalibScale = 1.41f;
#else
	volatile int16_t vCalibOffset = -4190;
	volatile float vCalibScale = 1.24f;
#endif
volatile uint16_t CalibZeroPos = 9985;

uint16_t DrawBuffer[2][(DRAWHEIGHT + 1) * DRAWBUFFERWIDTH];
volatile uint16_t adcA, adcB, adcC, oldAdc, capturePos = 0;
volatile bool drawing = false;
volatile uint8_t captureBufferNumber = 0, drawBufferNumber = 0;
volatile uint16_t ADC_array[ADC_BUFFER_LENGTH];
mode displayMode = Oscilloscope;
volatile uint32_t debugCount = 0, coverageTimer = 0, coverageTotal = 0, MIDIDebug = 0;


LCD lcd;
FFT fft;
UI ui;
MIDIHandler midi;
Osc osc;
Config cfg;


inline uint16_t CalcZeroSize() {					// returns ADC size that corresponds to 0v
	return (8192 - vCalibOffset) / vCalibScale;
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

	lcd.Init();								// Initialize ILI9341 LCD

	// check if resetting config by holding left encoder button while resetting
	if (L_BTN_GPIO->IDR & L_BTN_NO(GPIO_IDR_IDR_,))
		cfg.RestoreConfig();

	InitSampleAcquisition();
	ui.ResetMode();
	CalibZeroPos = CalcZeroSize();


	while (1) {

		ui.handleEncoders();

		if (cfg.scheduleSave && SysTickVal > cfg.saveBooked + 170000)			// this equates to around 60 seconds between saves
			cfg.SaveConfig();

		if (ui.menuMode) {

		} else if (displayMode == Oscilloscope) {
			osc.OscRun();

		} else if (displayMode == Fourier || displayMode == Waterfall) {
			fft.Run();

		} else if (displayMode == Circular) {
			osc.CircRun();

		} else if (displayMode == MIDI) {
			midi.ProcessMidi();

		}
	}
}
