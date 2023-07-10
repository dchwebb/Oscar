#include "config.h"
#include "ui.h"
#include "osc.h"
#include "fft.h"
#include "tuner.h"

// called whenever a config setting is changed to schedule a save after waiting to see if any more changes are being made
void Config::ScheduleSave()
{
	scheduleSave = true;
	saveBooked = SysTickVal;
}


// Write calibration settings to Flash memory
void Config::SaveConfig() {
	scheduleSave = false;

	uint32_t address = ADDR_FLASH_SECTOR_7;		// Store data in Sector 7 last sector in F446 to allow maximum space for program code
	FLASH_Status flash_status = FLASH_COMPLETE;

	uint32_t cfgSize = SetConfig();

	__disable_irq();		// Disable Interrupts
	FLASH_Unlock();			// Unlock Flash memory for writing

	// Clear error flags in Status Register
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR |FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR |FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

	// Erase sector 7 (has to be erased before write - this sets all bits to 1 as write can only switch to 0)
	flash_status = FLASH_EraseSector(FLASH_Sector_7, VoltageRange_3);

	// If erase worked, program the Flash memory with the config settings byte by byte
	if (flash_status == FLASH_COMPLETE) {
		for (unsigned int f = 0; f < cfgSize; f++) {
			char byte = *((char*)(&configBuffer) + f);
			flash_status = FLASH_ProgramByte((uint32_t)address + f, byte);
		}
	}

	FLASH_Lock();			// Lock the Flash memory
	__enable_irq(); 		// Enable Interrupts
}


uint32_t Config::SetConfig()
{
	// Serialise config values into buffer
	memset(configBuffer, 0, sizeof(configBuffer));					// Clear buffer
	strncpy(reinterpret_cast<char*>(configBuffer), "CFG", 4);		// Header
	configBuffer[4] = configVersion;
	uint32_t configPos = 8;											// Position in buffer to store data
	uint32_t configSize = 0;										// Holds the size of each config buffer

	uint8_t* cfgBuffer = nullptr;

	// UI settings
	configSize = ui.SerialiseConfig(&cfgBuffer);
	memcpy(&configBuffer[configPos], cfgBuffer, configSize);
	configPos += configSize;

	// Oscilloscope settings
	configSize = osc.SerialiseConfig(&cfgBuffer);
	memcpy(&configBuffer[configPos], cfgBuffer, configSize);
	configPos += configSize;

	// FFT settings
	configSize = fft.SerialiseConfig(&cfgBuffer);
	memcpy(&configBuffer[configPos], cfgBuffer, configSize);
	configPos += configSize;

	// Tuner settings
	configSize = tuner.SerialiseConfig(&cfgBuffer);
	memcpy(&configBuffer[configPos], cfgBuffer, configSize);
	configPos += configSize;

	return configPos;
}


// Restore configuration settings from flash memory
void Config::RestoreConfig()
{
	uint8_t* flashConfig = reinterpret_cast<uint8_t*>(ADDR_FLASH_SECTOR_7);

	// Check for config start and version number
	if (strcmp((char*)flashConfig, "CFG") == 0 && *reinterpret_cast<uint32_t*>(&flashConfig[4]) == configVersion) {
		uint32_t configPos = 8;											// Position in buffer of start of data

		configPos += ui.StoreConfig(&flashConfig[configPos]);
		configPos += osc.StoreConfig(&flashConfig[configPos]);
		configPos += fft.StoreConfig(&flashConfig[configPos]);
		configPos += tuner.StoreConfig(&flashConfig[configPos]);

	}
}



