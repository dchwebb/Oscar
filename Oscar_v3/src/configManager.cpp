#include "configManager.h"
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>

bool Config::SaveConfig(const bool forceSave)
{
	bool result = true;
	if (forceSave || (scheduleSave && SysTickVal > saveBooked + 30000)) {			// 30 seconds between saves FIXME

		scheduleSave = false;

		if (currentSettingsOffset == -1) {					// Default = -1 if not first set in RestoreConfig
			currentSettingsOffset = 0;
		} else {
			currentSettingsOffset += settingsSize;
			if ((uint32_t)currentSettingsOffset > flashSectorSize - settingsSize) {
				// Check if another sector is available for use
				for (auto& sector : sectors) {
					if (!sector.dirty && sector.sector != currentSector) {
						if (++currentIndex > configSectorCount) { 	// set new index, wrapping at count of allowed config sectors
							currentIndex = 0;
						}
						currentSector = sector.sector;
						SetCurrentConfigAddr();
						currentSettingsOffset = 0;
						sector.index = currentIndex;
						sector.dirty = true;
						break;
					}
				}
				if (currentSettingsOffset != 0) {				// No free configurations slots found - abort save
					printf("Error saving config - no space\r\n");
					return false;
				}
			}
		}
		uint32_t* flashPos = flashConfigAddr + currentSettingsOffset / 4;


		uint8_t configBuffer[settingsSize];					// Will hold all the data to be written by config savers
		memcpy(configBuffer, ConfigHeader, 4);
		configBuffer[4] = currentIndex;						// Store the index of the config to identify the active sector
		uint32_t configPos = headerSize;
		for (auto& saver : configSavers) {					// Add individual config settings to buffer after header
			memcpy(&configBuffer[configPos], saver->settingsAddress, saver->settingsSize);
			configPos += saver->settingsSize;
		}

		FlashUnlock();										// Unlock Flash memory for writing
		FLASH->SR = flashAllErrors;							// Clear error flags in Status Register
		result = FlashProgram(flashPos, reinterpret_cast<uint32_t*>(&configBuffer), settingsSize);
		FlashLock();										// Lock Flash

		if (result) {
			printf("Config Saved (%lu bytes at %#010lx)\r\n", settingsSize, (uint32_t)flashPos);
		} else {
			printf("Error saving config\r\n");
		}
	}
	return result;
}


void Config::RestoreConfig()
{


	// Initialise sector array - used to manage which sector contains current config, and which sectors are available for writing when current sector full
	for (uint32_t i = 0; i < configSectorCount; ++i) {
		sectors[i].sector = flashConfigSector + i;
		uint32_t* const addr = flashConfigAddr + i * (flashSectorSize / 4);

		// Check if sector is dirty
		for (uint32_t w = 0; w < (flashSectorSize / 4); ++w) {
			if (addr[w] != 0xFFFFFFFF) {
				sectors[i].dirty = true;
				break;
			}
		}

		// Check if there is a config block at the start of the sector and read the index number if so
		sectors[i].index = (addr[0] == *(uint32_t*)ConfigHeader) ? (uint8_t)addr[1] : 255;
	}

	// Work out which is the active config sector: will be the highest index from the bottom before the sequence jumps
	std::sort(sectors.begin(), sectors.end(), [](const CfgSector& l, const CfgSector& r) { return l.index < r.index; });
	uint32_t index = sectors[0].index;
	if (index == 255) {
		currentSector = flashConfigSector;
		currentIndex = 0;					// Each sector is assigned an index to determine which contains the latest config
	} else {
		currentSector = sectors[0].sector;
		for (uint32_t i = 1; i < configSectorCount; ++i) {
			if (sectors[i].index == index + 1) {
				++index;
				currentSector = sectors[i].sector;
			} else {
				break;
			}
		}
		currentIndex = index;
	}
	SetCurrentConfigAddr();				// Set the base address of the sector holding the current config

	// Erase any dirty sectors that are not the current one, or do not contain config data
	for (auto& sector : sectors) {
		if (sector.dirty && (sector.sector != currentSector || sector.index == 255)) {
			FlashEraseSector(sector.sector);
			sector.index = 255;
			sector.dirty = false;
		}
	}
	std::sort(sectors.begin(), sectors.end(), [](const CfgSector& l, const CfgSector& r) { return l.sector < r.sector; });

	// Locate latest (active) config block
	uint32_t pos = 0;
	while (pos <= flashSectorSize - settingsSize) {
		if (*(flashConfigAddr + pos / 4) == *(uint32_t*)ConfigHeader) {
			currentSettingsOffset = pos;
			pos += settingsSize;
		} else {
			break;			// Either reached the end of the sector or found the latest valid config block
		}
	}

	if (currentSettingsOffset >= 0) {
		const uint8_t* flashConfig = reinterpret_cast<uint8_t*>(flashConfigAddr) + currentSettingsOffset;
		uint32_t configPos = headerSize;		// Position in buffer to retrieve settings from

		// Restore settings
		for (auto saver : configSavers) {
			memcpy(saver->settingsAddress, &flashConfig[configPos], saver->settingsSize);
			if (saver->validateSettings != nullptr) {
				saver->validateSettings();
			}
			configPos += saver->settingsSize;
		}
	} else {		// If no config stored run update settings to initialise config across components
		for (auto saver : configSavers) {
			if (saver->validateSettings != nullptr) {
				saver->validateSettings();
			}
		}
	}
}


void Config::EraseConfig()
{
	for (uint32_t i = 0; i < configSectorCount; ++i) {
		FlashEraseSector(flashConfigSector + i);
		sectors[i].dirty = false;
		sectors[i].index = 255;
	}

	printf("Config Erased\r\n");
}


void Config::ScheduleSave()
{
	// called whenever a config setting is changed to schedule a save after waiting to see if any more changes are being made
	scheduleSave = true;
	saveBooked = SysTickVal;
}


void Config::FlashUnlock()
{
	// Unlock the FLASH control register access
	if ((FLASH->CR & FLASH_CR_LOCK) != 0)  {
		FLASH->KEYR = 0x45670123U;						// These magic numbers unlock the flash for programming
		FLASH->KEYR = 0xCDEF89ABU;
	}
}


void Config::FlashLock()
{
	FLASH->CR |= FLASH_CR_LOCK;							// Lock the FLASH Registers access
}


void Config::FlashEraseSector(uint8_t sector)
{
	FlashUnlock();										// Unlock Flash memory for writing
	FLASH->SR = flashAllErrors;							// Clear error flags in Status Register

	FLASH->CR &= ~FLASH_CR_PSIZE_Msk;
	FLASH->CR |= FLASH_CR_PSIZE_1;						// Set the erase programming size to 32bit (corresponds to voltage level 3 - ie 3.3V with no external programming voltage)
	FLASH->CR &= ~FLASH_CR_SNB_Msk;						// Clear Sector number
	FLASH->CR |= sector << FLASH_CR_SNB_Pos;			// Sector number selection
	FLASH->CR |= FLASH_CR_SER;							// Sector erase
	FLASH->CR |= FLASH_CR_STRT;

	FlashWaitForLastOperation();
	FLASH->CR &= ~FLASH_CR_SER;

	FlashLock();										// Lock Flash
}


bool Config::FlashWaitForLastOperation()
{
	if (FLASH->SR & flashAllErrors) {					// If any error occurred abort
		FLASH->SR = flashAllErrors;						// Clear error flags in Status Register
		return false;
	}

	while ((FLASH->SR & FLASH_SR_BSY) == FLASH_SR_BSY) {}

	if ((FLASH->SR & FLASH_SR_EOP) == FLASH_SR_EOP) {	// Check End of Operation flag
		FLASH->SR = FLASH_SR_EOP;						// Clear FLASH End of Operation pending bit
	}

	return true;
}


bool Config::FlashProgram(uint32_t* dest_addr, uint32_t* src_addr, size_t size)
{
	if (!FlashWaitForLastOperation()) {
		return false;
	}
	FLASH->CR |= FLASH_CR_PG;
	FLASH->CR |= FLASH_CR_PSIZE_1;						// Program size one word

	__ISB();
	__DSB();

	// Each write block is 32 bits
	for (uint16_t b = 0; b < std::ceil(static_cast<float>(size) / 4); ++b) {
		*dest_addr = *src_addr;
		++dest_addr;
		++src_addr;

		if (!FlashWaitForLastOperation()) {
			FLASH->CR &= ~FLASH_CR_PG;					// Clear programming flag
			return false;
		}
	}

	__ISB();
	__DSB();

	FLASH->CR &= ~FLASH_CR_PG;							// Clear programming flag
	return true;
}


