#include <config.h>

// called whenever a config setting is changed to schedule a save after waiting to see if any more changes are being made
void Config::ScheduleSave() {
	scheduleSave = true;
	saveBooked = SysTickVal;
}

// Write calibration settings to Flash memory
void Config::SaveConfig() {
	scheduleSave = false;

	uint32_t address = ADDR_FLASH_SECTOR_7;		// Store data in Sector 7 last sector in F446 to allow maximum space for program code
	FLASH_Status flash_status = FLASH_COMPLETE;

	configValues cv;
	SetConfig(cv);

	__disable_irq();		// Disable Interrupts
	FLASH_Unlock();			// Unlock Flash memory for writing

	// Clear error flags in Status Register
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR |FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR |FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

	// Erase sector 7 (has to be erased before write - this sets all bits to 1 as write can only switch to 0)
	flash_status = FLASH_EraseSector(FLASH_Sector_7, VoltageRange_3);

	// If erase worked, program the Flash memory with the config settings byte by byte
	if (flash_status == FLASH_COMPLETE) {
		for (unsigned int f = 0; f < sizeof(cv); f++) {
			char byte = *((char*)(&cv) + f);
			flash_status = FLASH_ProgramByte((uint32_t)address + f, byte);
		}
	}

	FLASH_Lock();			// Lock the Flash memory
	__enable_irq(); 		// Enable Interrupts
}

void Config::SetConfig(configValues &cv) {
	cv.gen_displayMode = displayMode;
	cv.gen_vCalibOffset = vCalibOffset;
	cv.gen_vCalibScale = vCalibScale;

	cv.osc_TriggerX = osc.triggerX;
	cv.osc_TriggerY = osc.triggerY;
	cv.osc_TriggerChannel = (osc.triggerChannel == channelA ? 1 : osc.triggerChannel == channelB ? 2 : osc.triggerChannel == channelC ? 3 : 0);
	cv.osc_EncModeL = osc.encModeL;
	cv.osc_EncModeR = osc.encModeR;
	cv.osc_SampleTimer = osc.sampleTimer;
	cv.osc_oscDisplay = osc.oscDisplay;
	cv.osc_multiLane = osc.multiLane;
	cv.osc_voltScale = osc.voltScale;

	cv.fft_autoTune = fft.autoTune;
	cv.fft_traceOverlay = fft.traceOverlay;
	cv.fft_channel = fft.channel;
	cv.fft_EncModeL = fft.EncModeL;
	cv.fft_EncModeR = fft.EncModeR;
}


// Restore configuration settings from flash memory
void Config::RestoreConfig()
{
	// create temporary copy of settings from memory to check if they are valid
	configValues cv;
	memcpy((uint32_t*)&cv, (uint32_t*)ADDR_FLASH_SECTOR_7, sizeof(cv));

	if (strcmp(cv.StartMarker, "CFG") == 0 && strcmp(cv.EndMarker, "END") == 0 && cv.Version == 2) {
		displayMode = cv.gen_displayMode;
		vCalibOffset = cv.gen_vCalibOffset;
		vCalibScale = cv.gen_vCalibScale;

		osc.triggerX = cv.osc_TriggerX;
		osc.triggerY = cv.osc_TriggerY;
		osc.triggerChannel = (cv.osc_TriggerChannel == 1 ? channelA : cv.osc_TriggerChannel == 2 ? channelB : cv.osc_TriggerChannel == 3 ? channelC : channelNone);
		osc.encModeL = cv.osc_EncModeL;
		osc.encModeR = cv.osc_EncModeR;
		osc.sampleTimer = cv.osc_SampleTimer;
		osc.oscDisplay = cv.osc_oscDisplay;
		osc.multiLane = cv.osc_multiLane;
		osc.voltScale = cv.osc_voltScale;

		fft.autoTune = cv.fft_autoTune;
		fft.traceOverlay = cv.fft_traceOverlay;
		fft.channel = cv.fft_channel;
		fft.EncModeL = cv.fft_EncModeL;
		fft.EncModeR = cv.fft_EncModeR;
	}
}


