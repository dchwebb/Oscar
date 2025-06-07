#pragma once

#include "initialisation.h"
#include <vector>
#include <array>

// Struct added to classes that need settings saved
struct ConfigSaver {
	void* settingsAddress;
	uint32_t settingsSize;
	void (*validateSettings)(void);		// function pointer to method that will validate config settings when restored
};


class Config {
	friend class CDCHandler;					// Allow the serial handler access to private data for printing

public:
	static constexpr uint8_t configVersion = 4;
	
	// STM32F446 has up to 8 sectors of flash organized as: 16K (sector 0-3), 64k (sector 4), 128k (sector 5-7)
	static constexpr uint32_t flashConfigSector = 6;
	static constexpr uint32_t flashSectorSize = 131072;		// 128 K
	static constexpr uint32_t configSectorCount = 1;		// Number of sectors after base sector used for config
	uint32_t* flashConfigAddr = reinterpret_cast<uint32_t* const>(FLASH_BASE + flashSectorSize * (flashConfigSector - 4));;

	bool scheduleSave = false;
	uint32_t saveBooked = false;

	// Constructor taking multiple config savers: Get total config block size from each saver
	Config(std::initializer_list<ConfigSaver*> initList) : configSavers(initList) {
		for (auto saver : configSavers) {
			settingsSize += saver->settingsSize;
		}
		// Ensure config size (+ 4 byte header + 1 byte index) is aligned to 8 byte boundary
		settingsSize = AlignTo16Bytes(settingsSize + headerSize);
	}

	void ScheduleSave();				// called whenever a config setting is changed to schedule a save after waiting to see if any more changes are being made
	bool SaveConfig(const bool forceSave = false);
	void EraseConfig();					// Erase flash page containing config
	void RestoreConfig();				// gets config from Flash, checks and updates settings accordingly

private:
	static constexpr uint32_t flashAllErrors = FLASH_SR_EOP | FLASH_SR_SOP |FLASH_SR_WRPERR | FLASH_SR_PGAERR |FLASH_SR_PGPERR | FLASH_SR_PGSERR;

	const std::vector<ConfigSaver*> configSavers;
	uint32_t settingsSize = 0;			// Size of all settings from each config saver module + size of config header

	const char ConfigHeader[4] = {'C', 'F', 'G', configVersion};
	static constexpr uint32_t headerSize = sizeof(ConfigHeader) + 1;
	int32_t currentSettingsOffset = -1;	// Offset within flash page to block containing active/latest settings

	uint32_t currentIndex = 0;			// Each config gets a new index to track across multiple sectors
	uint32_t currentSector = flashConfigSector;			// Sector containing current config
	struct CfgSector {
		uint32_t sector;
		uint8_t index;
		bool dirty;
	};
	std::array<CfgSector, configSectorCount> sectors;

	void SetCurrentConfigAddr() {
		flashConfigAddr = reinterpret_cast<uint32_t* const>(FLASH_BASE + flashSectorSize * (currentSector - 4));
	}
	void FlashUnlock();
	void FlashLock();
	void FlashEraseSector(uint8_t Sector);
	bool FlashWaitForLastOperation();
	bool FlashProgram(uint32_t* dest_addr, uint32_t* src_addr, size_t size);

	static const inline uint32_t AlignTo16Bytes(uint32_t val) {
		val += 15;
		val >>= 4;
		val <<= 4;
		return val;
	}
};

extern Config config;
