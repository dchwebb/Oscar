#pragma once

#include "initialisation.h"
#include "../drivers/stm32f4xx_flash.h"

#define ADDR_FLASH_SECTOR_7		((uint32_t)0x08060000) // Base address of Sector 7, 128 Kbytes


// Class used to store calibration settings - note this uses the Standard Peripheral Driver code
class Config {
public:
	bool scheduleSave = false;
	uint32_t saveBooked;

	void ScheduleSave();				// called whenever a config setting is changed to schedule a save after waiting to see if any more changes are being made
	void SaveConfig();
	void RestoreConfig();				// Gets config from Flash, checks and updates settings accordingly

private:
	uint32_t SetConfig();				// Serialise config values

	static constexpr uint32_t configVersion = 1;
	static constexpr uint32_t BufferSize = 200;
	uint8_t configBuffer[BufferSize];

};

extern Config cfg;
