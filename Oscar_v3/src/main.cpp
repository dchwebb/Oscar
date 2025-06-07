#include <MidiEvents.h>
#include "initialisation.h"
#include "configManager.h"
#include "ui.h"
#include "lcd.h"
#include "fft.h"
#include "osc.h"
#include "tuner.h"
#include "USB.h"

volatile uint32_t SysTickVal = 0;


extern "C" {
	#include "interrupts.h"
}

UI ui;
Osc osc;
FFT fft;
Tuner tuner;

Config config{&ui.configSaver, &osc.configSaver, &fft.configSaver, &tuner.configSaver};		// Construct config handler with list of configSavers


int main(void)
{
	InitHardware();
	lcd.Init();								// Initialize ILI9341 LCD

	// check if resetting config by holding menu button while resetting
	if (ui.btnMenu.pin.IsHigh()) {			// If button is NOT pressed reload config
		config.RestoreConfig();				// Restore calibration settings from flash memory
	}

	InitSampleAcquisition();
	ui.ResetMode();
	osc.CalcZeroSize();
	usb.Init(false);
	//InitWatchdog();

	while (1) {
		//ResetWatchdog();					// Reloads watchdog counter to prevent hardware reset
		ui.handleEncoders();
		config.SaveConfig();				// Save any scheduled changes
		usb.cdc.ProcessCommand();			// Check for incoming CDC commands


		if (ui.menuMode != UI::MenuMode::off) {

		} else if (ui.cfg.displayMode == DispMode::Oscilloscope) {
			osc.OscRun();
		} else if (ui.cfg.displayMode == DispMode::Tuner) {
			tuner.Run();
		} else if (ui.cfg.displayMode == DispMode::Fourier || ui.cfg.displayMode == DispMode::Waterfall) {
			fft.Run();
		} else if (ui.cfg.displayMode == DispMode::MIDI) {
			midiEvents.ProcessMidi();
		}

	}
}
