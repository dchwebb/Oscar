#include "lcd.h"



void Lcd::Init(void) {

	// Force reset
	LCD_RST_RESET;
	Delay(20000);
	LCD_RST_SET;
	Delay(20000);

	// Software reset
	Command(ILI9341_RESET);
	Delay(50000);

	CommandData(CDARGS {ILI9341_POWERA, 0x39, 0x2C, 0x00, 0x34, 0x02});
	CommandData(CDARGS {ILI9341_POWERB, 0x00, 0xC1, 0x30});
	CommandData(CDARGS {ILI9341_DTCA, 0x85, 0x00, 0x78});
	CommandData(CDARGS {ILI9341_DTCB, 0x00,	0x00});
	CommandData(CDARGS {ILI9341_POWER_SEQ, 0x64, 0x03, 0x12, 0x81});
	CommandData(CDARGS {ILI9341_PRC, 0x20});
	CommandData(CDARGS {ILI9341_POWER1,	0x23});
	CommandData(CDARGS {ILI9341_POWER2,	0x10});
	CommandData(CDARGS {ILI9341_VCOM1, 0x3E, 0x28});
	CommandData(CDARGS {ILI9341_VCOM2, 0x86});
	CommandData(CDARGS {ILI9341_MAC, 0x48});
	CommandData(CDARGS {ILI9341_PIXEL_FORMAT, 0x55});
	CommandData(CDARGS {ILI9341_PIXEL_FORMAT, 0x55});
	CommandData(CDARGS {ILI9341_FRC, 0x00, 0x18});
	CommandData(CDARGS {ILI9341_DFC, 0x08, 0x82, 0x27});
	CommandData(CDARGS {ILI9341_3GAMMA_EN, 0x00});
	CommandData(CDARGS {ILI9341_COLUMN_ADDR, 0x00, 0x00, 0x00, 0xEF});
	CommandData(CDARGS {ILI9341_PAGE_ADDR, 0x00, 0x00, 0x01, 0x3F});
	CommandData(CDARGS {ILI9341_GAMMA, 0x01});
	CommandData(CDARGS {ILI9341_PGAMMA,	0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00});
	CommandData(CDARGS {ILI9341_NGAMMA, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F});
	Command(ILI9341_SLEEP_OUT);

	Delay(1000000);

	Command(ILI9341_DISPLAY_ON);
	Command(ILI9341_GRAM);
};

void Lcd::CommandData(CDARGS cmds) {
	Command(cmds[0]);
	for (uint8_t i = 1; i < cmds.size(); ++i)
		Data(cmds[i]);
}

void Lcd::Delay(volatile uint32_t delay) {
	for (; delay != 0; delay--);
}

void Lcd::Command(uint8_t data) {
	LCD_WRX_RESET;
	LCD_CS_RESET;
	SPISendByte(data);
	LCD_CS_SET;
}

void Lcd::Data(uint8_t data) {
	LCD_WRX_SET;
	LCD_CS_RESET;
	SPISendByte(data);
	LCD_CS_SET;
}

inline void Lcd::SPISendByte(uint8_t data) {
	while ((SPI5->SR & (SPI_SR_TXE | SPI_SR_RXNE)) == 0 || (SPI5->SR & SPI_SR_BSY));	// Wait for previous transmissions to complete if DMA TX enabled for SPI

	SPI5->DR = data;					// Fill output buffer with data

	while ((SPI5->SR & (SPI_SR_TXE | SPI_SR_RXNE)) == 0 || (SPI5->SR & SPI_SR_BSY));	// Wait for transmission to complete
}

void Lcd::Rotate(LCD_Orientation_t orientation) {
	Command(ILI9341_MAC);
	if (orientation == LCD_Portrait) {
		Data(0x58);
	} else if (orientation == LCD_Portrait_Flipped) {
		Data(0x88);
	} else if (orientation == LCD_Landscape) {
		Data(0x28);
	} else if (orientation == LCD_Landscape_Flipped) {
		Data(0xE8);
	}

	/*if (orientation == LCD_Portrait || orientation == LCD_Portrait_Flipped) {
		ILI9341_Opts.width = ILI9341_WIDTH;
		ILI9341_Opts.height = ILI9341_HEIGHT;
		ILI9341_Opts.orientation = TM_ILI9341_Portrait;
	} else {
		ILI9341_Opts.width = ILI9341_HEIGHT;
		ILI9341_Opts.height = ILI9341_WIDTH;
		ILI9341_Opts.orientation = TM_ILI9341_Landscape;
	}*/
}
