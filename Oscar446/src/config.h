#pragma once

#include "initialisation.h"
#include "osc.h"
#include "fft.h"
#include "../drivers/stm32f4xx_flash.h"

#define ADDR_FLASH_SECTOR_7		((uint32_t)0x08060000) // Base address of Sector 7, 128 Kbytes

class FFT;		// forward reference to handle circular dependency
class Osc;
class LCD;
extern FFT fft;
extern LCD lcd;
extern Osc osc;



struct configValues {
	char StartMarker[4] = "CFG";		// Start Marker

	//	General settings
	uint8_t Version = 2;				// version of saved config struct format
	DispMode gen_displayMode = DispMode::Oscilloscope;
	int16_t gen_vCalibOffset;
	float gen_vCalibScale;

	// oscilloscope settings
	int16_t osc_TriggerX;
	int32_t osc_TriggerY;
	uint8_t osc_TriggerChannel;
	encoderType osc_EncModeL;
	encoderType osc_EncModeR;
	uint16_t osc_SampleTimer;
	int8_t osc_oscDisplay;
	bool osc_multiLane;
	int8_t osc_voltScale;

	// FFT settings
	bool fft_autoTune;
	bool fft_traceOverlay;
	oscChannel fft_channel;
	encoderType fft_EncModeL;
	encoderType fft_EncModeR;

	char EndMarker[4] = "END";			// End Marker
};


// Class used to store calibration settings - note this uses the Standard Peripheral Driver code
class Config {
public:
	bool scheduleSave = false;
	uint32_t saveBooked;

	void ScheduleSave();				// called whenever a config setting is changed to schedule a save after waiting to see if any more changes are being made
	void SaveConfig();
	void SetConfig(configValues &cv);	// sets properties of class to match current values
	void RestoreConfig();				// gets config from Flash, checks and updates settings accordingly

};

