#pragma once

#include "stm32f4xx.h"
#include "fontData.h"
#include <vector>
#include <string>

#define DRAWWIDTH 320
#define DRAWHEIGHT 215

// RGB565 colours
#define LCD_WHITE		0xFFFF
#define LCD_BLACK		0x0000
#define LCD_GREY		0x528A
#define LCD_RED			0xF800
#define LCD_GREEN		0x07E0
#define LCD_GREEN2		0xB723
#define LCD_BLUE		0x001F
#define LCD_LIGHTBLUE	0x051D
#define LCD_YELLOW		0xFFE0
#define LCD_ORANGE		0xFBE4
#define LCD_CYAN		0x07FF
#define LCD_MAGENTA		0xA254
#define LCD_GRAY		0x7BEF
#define LCD_BROWN		0xBBCA

// LCD command definitions
#define ILI9341_RESET				0x01
#define ILI9341_SLEEP_MODE			0x11
#define ILI9341_GAMMA				0x26
#define ILI9341_DISPLAY_OFF			0x28
#define ILI9341_DISPLAY_ON			0x29
#define ILI9341_COLUMN_ADDR			0x2A
#define ILI9341_PAGE_ADDR			0x2B
#define ILI9341_GRAM				0x2C
#define ILI9341_MAC					0x36
#define ILI9341_PIXEL_FORMAT		0x3A
#define ILI9341_WDB					0x51
#define ILI9341_WCD					0x53
#define ILI9341_RGB_INTERFACE		0xB0
#define ILI9341_FRC					0xB1
#define ILI9341_BPC					0xB5
#define ILI9341_DFC					0xB6
#define ILI9341_POWER1				0xC0
#define ILI9341_POWER2				0xC1
#define ILI9341_VCOM1				0xC5
#define ILI9341_VCOM2				0xC7
#define ILI9341_POWERA				0xCB
#define ILI9341_POWERB				0xCF
#define ILI9341_PGAMMA				0xE0
#define ILI9341_NGAMMA				0xE1
#define ILI9341_DTCA				0xE8
#define ILI9341_DTCB				0xEA
#define ILI9341_POWER_SEQ			0xED
#define ILI9341_3GAMMA_EN			0xF2
#define ILI9341_INTERFACE			0xF6
#define ILI9341_PRC					0xF7

// Define macros for setting and clearing GPIO SPI pins
#define LCD_RST_RESET	GPIOD->BSRRH |= GPIO_BSRR_BS_12
#define LCD_RST_SET 	GPIOD->BSRRL |= GPIO_BSRR_BS_12
#define LCD_DCX_RESET	GPIOD->BSRRH |= GPIO_BSRR_BS_13
#define LCD_DCX_SET		GPIOD->BSRRL |= GPIO_BSRR_BS_13

/*
// Chip select - active low. Shouldn't need to do anything with this pin
#define LCD_CS_RESET	GPIOC->BSRRH |= GPIO_BSRR_BS_2
#define LCD_CS_SET		GPIOC->BSRRL |= GPIO_BSRR_BS_2
*/

// Macro for creating arguments to CommandData function
#define CDARGS			std::vector<uint8_t>

// Macros to check if DMA or SPI are busy - shouldn't need to check Stream5 as this is receive
#define SPI_DMA_Working	DMA2_Stream5->NDTR || DMA2_Stream6->NDTR || ((SPI5->SR & (SPI_SR_TXE | SPI_SR_RXNE)) == 0 || (SPI5->SR & SPI_SR_BSY))
//#define SPI_Working			(SPI5->SR & (SPI_SR_TXE | SPI_SR_RXNE)) == 0 || (SPI5->SR & SPI_SR_BSY)
//#define SPI_DMA_Working		DMA2_Stream6->NDTR || (SPI5->SR & SPI_SR_TXE) == 0
//#define SPI_Working			(SPI5->SR & SPI_SR_TXE) == 0 || (SPI5->SR & SPI_SR_BSY)

typedef enum {
	LCD_Portrait, 			// Portrait
	LCD_Portrait_Flipped, 	// Portrait flipped
	LCD_Landscape,			// Landscape
	LCD_Landscape_Flipped	// Landscape flipped
} LCD_Orientation_t;

typedef enum {
	SPIDataSize_8b,			// SPI in 8-bits mode
	SPIDataSize_16b 		// SPI in 16-bits mode
} SPIDataSize_t;

struct FontData {
	const uint8_t Width;    // Font width in pixels
	const uint8_t Height;   // Font height in pixels
	const uint16_t *data;	// Pointer to data font data array
};


class LCD {
public:
	LCD_Orientation_t orientation = LCD_Portrait;
	uint16_t width = 240;
	uint16_t height = 320;
	uint16_t DMAint16;
	FontData Font_Small {7, 10, Font7x10};
	FontData Font_Large {11, 18, Font11x18};
/*
	FontData Font_Medium {12, 12, Font12x12};

	FontData Font_XLarge {16, 26, Font16x26};
*/

	LCD();
	void Init(void);
	void Rotate(LCD_Orientation_t orientation);
	void ScreenFill(const uint16_t& colour);
	void ColourFill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const uint16_t& colour);
	void PatternFill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const uint16_t* PixelData);
	void DrawPixel(uint16_t x, uint16_t y, const uint16_t& colour);
	void DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const uint32_t& colour);
	void DrawRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const uint32_t& colour);
	void DrawChar(uint16_t x, uint16_t y, char c, const FontData *font, const uint32_t& foreground, const uint32_t& background);
	void DrawCharMem(uint16_t x, uint16_t y, uint16_t memWidth, uint16_t* memBuffer, char c, const FontData *font, const uint32_t& foreground, const uint32_t& background);
	void DrawString(uint16_t x0, uint16_t y0, std::string s, const FontData *font, const uint32_t& foreground, const uint32_t& background);
	void DrawStringMem(uint16_t x0, uint16_t y0, uint16_t memWidth, uint16_t* memBuffer, std::string s, const FontData *font, const uint32_t& foreground, const uint32_t& background);

	void Command(const uint8_t& data);

private:

	uint16_t charBuffer[2][16 * 26];
	uint8_t currentCharBuffer = 0;

	void Delay(volatile uint32_t delay);
	void Data(const uint8_t& data);
	void Data16b(const uint16_t& data);
	void CommandData(CDARGS);
	void SetCursorPosition(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

	inline void SPISendByte(const uint8_t data);
	void SPISetDataSize(const SPIDataSize_t& Mode);
	void SPI_DMA_SendHalfWord(const uint16_t& value, const uint16_t& count);

};


