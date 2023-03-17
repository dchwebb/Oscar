#pragma once

#include "initialisation.h"
#include "fontData.h"
#include <string_view>


// RGB565 colours
#define LCD_WHITE		0xFFFF
#define LCD_BLACK		0x0000
#define LCD_GREY		0x528A
#define LCD_RED			0xF800
#define LCD_GREEN		0x07E0
#define LCD_DULLGREEN	0x02E0
#define LCD_BLUE		0x001F
#define LCD_LIGHTBLUE	0x051D
#define LCD_DULLBLUE	0x0293
#define LCD_YELLOW		0xFFE0
#define LCD_ORANGE		0xFBE4
#define LCD_DULLORANGE	0xA960
#define LCD_CYAN		0x07FF
#define LCD_MAGENTA		0xA254
#define LCD_GRAY		0x7BEF
#define LCD_BROWN		0xBBCA


// LCD command definitions
enum class cmdILI9341 : uint8_t {RESET = 0x01,
	SLEEP_MODE		= 0x11,
	GAMMA			= 0x26,
	DISPLAY_OFF		= 0x28,
	DISPLAY_ON		= 0x29,
	COLUMN_ADDR		= 0x2A,
	PAGE_ADDR		= 0x2B,
	GRAM			= 0x2C,
	MAC				= 0x36,
	PIXEL_FORMAT	= 0x3A,
	WDB				= 0x51,
	WCD				= 0x53,
	RGB_INTERFACE	= 0xB0,
	FRC				= 0xB1,
	BPC				= 0xB5,
	DFC				= 0xB6,
	POWER1			= 0xC0,
	POWER2			= 0xC1,
	VCOM1			= 0xC5,
	VCOM2			= 0xC7,
	POWERA			= 0xCB,
	POWERB			= 0xCF,
	PGAMMA			= 0xE0,
	NGAMMA			= 0xE1,
	DTCA			= 0xE8,
	DTCB			= 0xEA,
	POWER_SEQ		= 0xED,
	THREEGAMMA_EN	= 0xF2,
	INTERFACE		= 0xF6,
	PRC				= 0xF7
};

// Define LCD DMA and SPI registers
#define LCD_DMA_STREAM			DMA1_Stream5
#define LCD_SPI 				SPI3
#define LCD_CLEAR_DMA_FLAGS		DMA1->HIFCR = DMA_HIFCR_CHTIF5 | DMA_HIFCR_CTCIF5 | DMA_HIFCR_CTEIF5;

// Define macros for setting and clearing GPIO SPI pins
#define LCD_RST_RESET	GPIOC->BSRR |= GPIO_BSRR_BR_14
#define LCD_RST_SET 	GPIOC->BSRR |= GPIO_BSRR_BS_14
#define LCD_DCX_RESET	GPIOC->BSRR |= GPIO_BSRR_BR_13
#define LCD_DCX_SET		GPIOC->BSRR |= GPIO_BSRR_BS_13

// Macro for creating arguments to CommandData function
typedef std::vector<uint8_t> cdArgs_t;

// Macros to check if DMA or SPI are busy - shouldn't need to check Stream5 as this is receive
#define SPI_DMA_Working	LCD_DMA_STREAM->NDTR || ((LCD_SPI->SR & (SPI_SR_TXE | SPI_SR_RXNE)) == 0 || (LCD_SPI->SR & SPI_SR_BSY))

struct FontData {
	const uint8_t Width;    // Font width in pixels
	const uint8_t Height;   // Font height in pixels
	const uint16_t *data;	// Pointer to data font data array
};


class LCD {
public:
	uint16_t width = 240;
	uint16_t height = 320;

	static constexpr uint16_t drawWidth = 320;
	static constexpr uint16_t drawHeight = 216;
	static constexpr uint16_t drawBufferWidth = 106;		// Maximum width of draw buffer (3 * 106 = 318 which is two short of full width but used in FFT for convenience

	static constexpr FontData Font_Small {7, 10, Font7x10};
	static constexpr FontData Font_Large {11, 18, Font11x18};

	uint16_t drawBuffer[2][(drawHeight + 1) * drawBufferWidth];

	void Init(void);
	void ScreenFill(const uint16_t colour);
	void ColourFill(const uint16_t x0, const uint16_t y0, const uint16_t x1, const uint16_t y1, const uint16_t colour);
	void PatternFill(const uint16_t x0, const uint16_t y0, uint16_t x1, uint16_t y1, const uint16_t* PixelData);
	void DrawPixel(const uint16_t x, const uint16_t y, const uint16_t colour);
	void DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const uint16_t colour);
	void DrawRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const uint16_t colour);
	void DrawChar(uint16_t x, uint16_t y, char c, const FontData *font, const uint32_t& foreground, const uint16_t background);
	void DrawCharMem(uint16_t x, uint16_t y, uint16_t memWidth, uint16_t* memBuffer, char c, const FontData *font, const uint16_t foreground, const uint16_t background);
	void DrawString(uint16_t x0, const uint16_t y0, std::string_view s, const FontData *font, const uint16_t foreground, const uint16_t background);
	void DrawStringMem(uint16_t x0, const uint16_t y0, uint16_t memWidth, uint16_t* memBuffer, std::string_view s, const FontData *font, const uint16_t foreground, const uint16_t background);

private:
	enum LCD_Orientation_t { LCD_Portrait, LCD_Portrait_Flipped, LCD_Landscape, LCD_Landscape_Flipped } ;
	enum SPIDataSize_t { SPIDataSize_8b, SPIDataSize_16b };			// SPI in 8-bits mode/16-bits mode

	LCD_Orientation_t orientation = LCD_Portrait;
	uint16_t charBuffer[2][Font_Large.Width * Font_Large.Height];
	uint8_t currentCharBuffer = 0;
	uint16_t dmaInt16;										// Used to buffer data for DMA transfer during colour fills

	void Data(const uint8_t data);
	void Data16b(const uint16_t data);
	void Command(const cmdILI9341 data);
	void CommandData(const cmdILI9341 cmd, cdArgs_t data);
	void Rotate(LCD_Orientation_t orientation);
	void SetCursorPosition(const uint16_t x1, const uint16_t y1, const uint16_t x2, const uint16_t y2);
	void Delay(volatile uint32_t delay);

	inline void SPISendByte(const uint8_t data);
	void SPISetDataSize(const SPIDataSize_t& Mode);
	void SPI_DMA_SendHalfWord(const uint16_t& value, const uint16_t& count);

};


