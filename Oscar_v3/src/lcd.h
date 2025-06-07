#pragma once

#include "initialisation.h"
#include "fontData.h"
#include <string_view>
#include <vector>

// RGB565 colours
union RGBColour  {
	uint16_t colour;
	struct __attribute__((packed)) {
		uint16_t blue : 5;
		uint16_t green : 6;
		uint16_t red : 5;
	};

	constexpr RGBColour(uint8_t r, uint8_t g, uint8_t b) {
		colour = (r << 11) + (g << 5) + b;
	}
	constexpr RGBColour(int col) : colour {static_cast<uint16_t>(col)} {};
	constexpr RGBColour(uint16_t col) : colour {col} {};

	RGBColour DarkenColour(const uint16_t amount) const
	{
		//	Darken an RGB colour by the specified amount (apply bit offset so that all colours are treated as 6 bit values)
		const uint8_t r = (red << 1) - std::min(amount, (uint16_t)(red << 1));
		const uint8_t g = green - std::min(amount, green);
		const uint8_t b = (blue << 1) - std::min(amount, (uint16_t)(blue << 1));
		return RGBColour{uint8_t(r >> 1), g, uint8_t(b >> 1)};
	}

	enum colours : uint16_t {
		White = 0xFFFF,
		Black = 0x0000,
		Grey = 0x528A,
		LightGrey = 0x8C71,
		Red = 0xF800,
		Green = 0x07E0,
		DullGreen = 0x02E0,
		Blue = 0x001F,
		LightBlue = 0x051D,
		DullBlue = 0x0293,
		DarkBlue = 0x01cd,
		Yellow = 0xFFE0,
		Orange = 0xFB44,
		DullOrange = 0xA960,
		Cyan = 0x07FF,
		Magenta = 0xA254,
		Gray = 0x7BEF,
		Brown = 0xBBCA};
};


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

// Macros to check if DMA or SPI are busy - shouldn't need to check Stream5 as this is receive
#define SPI_DMA_Working	LCD_DMA_STREAM->NDTR || ((LCD_SPI->SR & (SPI_SR_TXE | SPI_SR_RXNE)) == 0 || (LCD_SPI->SR & SPI_SR_BSY))

struct FontData {
	const uint8_t Width;    // Font width in pixels
	const uint8_t Height;   // Font height in pixels
	const uint16_t *data;	// Pointer to data font data array
};


class LCD {
public:
	uint16_t width = 320;
	uint16_t height = 240;

	static constexpr uint16_t drawWidth = 320;
	static constexpr uint16_t drawHeight = 216;
	static constexpr uint16_t drawBufferWidth = 106;		// Maximum width of draw buffer (3 * 106 = 318 which is two short of full width but used in FFT for convenience

	static constexpr FontData Font_Small {7, 10, Font7x10};
	static constexpr FontData Font_Large {11, 18, Font11x18};
	static constexpr FontData Font_XLarge {16, 26, Font16x26};

	uint16_t drawBuffer[2][(drawHeight + 1) * drawBufferWidth];

	void Init(void);
	void ScreenFill(const uint16_t colour);
	void ColourFill(const uint16_t x0, const uint16_t y0, const uint16_t x1, const uint16_t y1, const RGBColour colour);
	void PatternFill(const uint16_t x0, const uint16_t y0, uint16_t x1, uint16_t y1, const uint16_t* PixelData);
	void DrawPixel(const uint16_t x, const uint16_t y, const RGBColour colour);
	void DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const RGBColour colour);
	void DrawRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const RGBColour colour);
	void DrawChar(uint16_t x, uint16_t y, char c, const FontData& font, const RGBColour& foreground, const RGBColour background);
	void DrawCharMem(uint16_t x, uint16_t y, uint16_t memWidth, uint16_t* memBuffer, char c, const FontData& font, const RGBColour foreground, const RGBColour background);
	void DrawString(uint16_t x0, const uint16_t y0, std::string_view s, const FontData& font, const RGBColour foreground, const RGBColour background);
	void DrawStringCenter(uint32_t xCenter, const uint16_t y0, const size_t maxWidth, std::string_view s, const FontData& font, const uint16_t foreground, const uint16_t background);
	void DrawStringMem(uint16_t x0, const uint16_t y0, uint16_t memWidth, uint16_t* memBuffer, std::string_view s, const FontData& font, const uint16_t foreground, const uint16_t background);
	void DrawStringMemCenter(uint16_t x0, const uint16_t y0, const size_t width, uint16_t* memBuffer, std::string_view s, const FontData& font, const uint16_t foreground, const uint16_t background);
private:
	enum LCD_Orientation_t { LCD_Portrait, LCD_Portrait_Flipped, LCD_Landscape, LCD_Landscape_Flipped } ;
	enum SPIDataSize_t { SPIDataSize_8b, SPIDataSize_16b };			// SPI in 8-bits mode/16-bits mode

	LCD_Orientation_t orientation = LCD_Portrait;
	uint16_t charBuffer[2][Font_XLarge.Width * Font_XLarge.Height];
	uint8_t currentCharBuffer = 0;
	uint16_t dmaInt16;										// Used to buffer data for DMA transfer during colour fills

	void Data(const uint8_t data);
	void Data16b(const uint16_t data);
	void Command(const cmdILI9341 data);
	void CommandData(const cmdILI9341 cmd, std::initializer_list<uint8_t> data);
	void Rotate(LCD_Orientation_t orientation);
	void SetCursorPosition(const uint16_t x1, const uint16_t y1, const uint16_t x2, const uint16_t y2);
	void Delay(volatile uint32_t delay);

	inline void SPISendByte(const uint8_t data);
	void SPISetDataSize(const SPIDataSize_t& Mode);
	void SPI_DMA_SendHalfWord(const uint16_t& value, const uint16_t& count);

	GpioPin lcdDCPin {GPIOA, 8, GpioPin::Type::Output, 0, GpioPin::DriveStrength::Medium};			// LCD DC (Data/Command) pin
	GpioPin lcdReset {GPIOA, 10, GpioPin::Type::Output};											// LCD Reset pin

};


extern LCD lcd;
