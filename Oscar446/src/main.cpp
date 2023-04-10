#include "initialisation.h"
#include "config.h"
#include "ui.h"
#include "lcd.h"
#include "fft.h"
#include "midi.h"
#include "osc.h"
#include "tuner.h"

extern uint32_t SystemCoreClock;
volatile uint32_t SysTickVal = 0;

int16_t vCalibOffset = -3590;				// Dev board with 14k resistors: -3940, 1.499999
float vCalibScale = 1.46f;
uint16_t calibZeroPos = 9985;

volatile uint16_t ADC_array[ADC_BUFFER_LENGTH];

uint32_t coverageTimer = 0, coverageTotal = 0;


LCD lcd;
UI ui;
MIDIHandler midi;
Osc osc;
Config cfg;



inline uint16_t CalcZeroSize() {			// returns ADC size that corresponds to 0v
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
	InitLCDHardware();
	InitADC();
	InitEncoders();
	InitUART();
	InitSysTick();
	lcd.Init();								// Initialize ILI9341 LCD

	// check if resetting config by holding left encoder button while resetting
	if (GPIOA->IDR & GPIO_IDR_IDR_10) {		// If button is NOT pressed reload config
		cfg.RestoreConfig();
	}

	InitSampleAcquisition();
	ui.ResetMode();
	calibZeroPos = CalcZeroSize();


	while (1) {

		ui.handleEncoders();

		if (cfg.scheduleSave && SysTickVal > cfg.saveBooked + 170000)			// this equates to around 60 seconds between saves
			cfg.SaveConfig();

		if (ui.menuMode) {

		} else if (ui.displayMode == DispMode::Oscilloscope) {
			osc.OscRun();

		} else if (ui.displayMode == DispMode::Tuner) {
			tuner.Run();

		} else if (ui.displayMode == DispMode::Fourier || ui.displayMode == DispMode::Waterfall) {
			fft.Run();

		} else if (ui.displayMode == DispMode::MIDI) {
			midi.ProcessMidi();

		}
	}
}
