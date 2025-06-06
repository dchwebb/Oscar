#include "lcd.h"

LCD lcd;

void LCD::Init()
{
	// Force reset
	lcdReset.SetLow();
	Delay(20000);
	lcdReset.SetHigh();
	Delay(20000);

	// Software reset
	Command(cmdILI9341::RESET);
	Delay(50000);

	CommandData(cmdILI9341::POWERA, {0x39, 0x2C, 0x00, 0x34, 0x02});
	CommandData(cmdILI9341::POWERB, {0x00, 0xC1, 0x30});
	CommandData(cmdILI9341::DTCA, {0x85, 0x00, 0x78});		// default is  0x85, 0x00, 0x78
	CommandData(cmdILI9341::DTCB, {0x00,	0x00});			// default is  0x66, 0x00
	CommandData(cmdILI9341::POWER_SEQ, {0x64, 0x03, 0x12, 0x81});		// default is 0x55 0x01 0x23 0x01
	CommandData(cmdILI9341::PRC, {0x20});					// 0x20: DDVDH = 2xVCI  (Pump ratio control)
	CommandData(cmdILI9341::POWER1, {0x23});				// 0x23: GVDD level = 4.6V (reference level for VCOM level and grayscale voltage level)
	CommandData(cmdILI9341::POWER2, {0x10});				// Appears to be invalid - should be 0b000 - 0b011
	CommandData(cmdILI9341::VCOM1, {0x3E, 0x28});			// 0x3E: VCOMH = 4.250V; 0x28: VCOML = 3.700V
	CommandData(cmdILI9341::VCOM2, {0x86});					// 0x86: VCOM offset voltage = VMH - 58 VML � 58;
	CommandData(cmdILI9341::MAC, {0x48});					// Memory access control: MX = Column Address Order, RGB-BGR Order control
	CommandData(cmdILI9341::PIXEL_FORMAT, {0x55});			// 16 bit format
	CommandData(cmdILI9341::FRC, {0x00, 0x18});
	CommandData(cmdILI9341::DFC, {0x08, 0x82, 0x27});
	CommandData(cmdILI9341::THREEGAMMA_EN, {0x00});
	CommandData(cmdILI9341::COLUMN_ADDR, {0x00, 0x00, 0x00, 0xEF});
	CommandData(cmdILI9341::PAGE_ADDR, {0x00, 0x00, 0x01, 0x3F});
	CommandData(cmdILI9341::GAMMA, {0x01});
	CommandData(cmdILI9341::PGAMMA, {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00});
	CommandData(cmdILI9341::NGAMMA, {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F});
	Command(cmdILI9341::SLEEP_MODE);

	Delay(1000000);

	Command(cmdILI9341::DISPLAY_ON);
	Command(cmdILI9341::GRAM);

#if defined(STM32F42_43xxx) || defined(STM32F446xx)
	Rotate(LCD_Landscape_Flipped);
#else
	Rotate(LCD_Landscape);
#endif
	ScreenFill(RGBColour::Black);
};

void LCD::CommandData(const cmdILI9341 cmd, std::initializer_list<uint8_t> data)
{
	Command(cmd);
	lcdDCPin.SetHigh();
	for (uint8_t d : data) {
		SPISendByte(d);
	}
}


void LCD::Delay(volatile uint32_t delay)		// delay must be declared volatile or will be optimised away
{
	for (; delay != 0; delay--);
}


void LCD::Command(const cmdILI9341 cmd)
{
	while (SPI_DMA_Working);
	lcdDCPin.SetLow();
	SPISendByte(static_cast<uint8_t>(cmd));
}


//	Send data in either 8 or 16 bit modes
void LCD::Data(const uint8_t data)
{
	lcdDCPin.SetHigh();
	SPISendByte(data);
}


void LCD::Data16b(const uint16_t data)
{
	lcdDCPin.SetHigh();
	SPISendByte(data >> 8);
	SPISendByte(data & 0xFF);
}


void LCD::SetCursorPosition(const uint16_t x1, const uint16_t y1, const uint16_t x2, const uint16_t y2)
{
	Command(cmdILI9341::COLUMN_ADDR);
	Data16b(x1);
	Data16b(x2);

	Command(cmdILI9341::PAGE_ADDR);
	Data16b(y1);
	Data16b(y2);
}


void LCD::Rotate(const LCD_Orientation_t o)
{
	Command(cmdILI9341::MAC);
	switch (o) {
		case LCD_Portrait :				Data(0x58); break;
		case LCD_Portrait_Flipped : 	Data(0x88); break;
		case LCD_Landscape : 			Data(0x28); break;
		case LCD_Landscape_Flipped :	Data(0xE8); break;
	}

	orientation = o;
	if (o == LCD_Portrait || o == LCD_Portrait_Flipped) {
		width = 240;
		height = 320;
	} else {
		width = 320;
		height = 240;
	}
}


void LCD::ScreenFill(const uint16_t colour)
{
	ColourFill(0, 0, width - 1, height - 1, colour);
}


void LCD::ColourFill(const uint16_t x0, const uint16_t y0, const uint16_t x1, const uint16_t y1, const RGBColour colour)
{
	const uint32_t pixelCount = (x1 - x0 + 1) * (y1 - y0 + 1);

	SetCursorPosition(x0, y0, x1, y1);
	dmaInt16 = colour.colour;								// data to transfer - set early to avoid problem where F722 doesn't update buffer until midway through send
	Command(cmdILI9341::GRAM);

	lcdDCPin.SetHigh();

	SPISetDataSize(SPIDataSize_16b);				// 16-bit SPI mode

	LCD_CLEAR_DMA_FLAGS
	LCD_DMA_STREAM->CR &= ~DMA_SxCR_MINC;			// Memory not in increment mode
	LCD_DMA_STREAM->NDTR = (pixelCount > 0xFFFF) ? pixelCount / 2 : pixelCount;						// Number of data items to transfer
	LCD_DMA_STREAM->M0AR = (uint32_t) &dmaInt16;	// DMA_InitStruct.DMA_Memory0BaseAddr;
	LCD_DMA_STREAM->CR |= DMA_SxCR_EN;				// Enable DMA transfer stream
	LCD_SPI->CR2 |= SPI_CR2_TXDMAEN;				// Enable SPI TX DMA

	if (pixelCount > 0xFFFF) {						// Send remaining data
		while (SPI_DMA_Working);
		LCD_CLEAR_DMA_FLAGS
		LCD_DMA_STREAM->NDTR = pixelCount / 2;		// Number of data items to transfer
		LCD_DMA_STREAM->CR |= DMA_SxCR_EN;			// Enable DMA transfer stream
	}
}


void LCD::PatternFill(const uint16_t x0, const uint16_t y0, const uint16_t x1, const uint16_t y1, const uint16_t* PixelData)
{
	uint32_t pixelCount = (x1 - x0 + 1) * (y1 - y0 + 1);

	SetCursorPosition(x0, y0, x1, y1);
	Command(cmdILI9341::GRAM);
	lcdDCPin.SetHigh();
	SPISetDataSize(SPIDataSize_16b);				// 16-bit SPI mode

	LCD_CLEAR_DMA_FLAGS
	LCD_DMA_STREAM->CR |= DMA_SxCR_MINC;			// Memory in increment mode
	LCD_DMA_STREAM->NDTR = pixelCount;				// Number of data items to transfer
	LCD_DMA_STREAM->M0AR = (uint32_t)PixelData;		// DMA_InitStruct.DMA_Memory0BaseAddr
	LCD_DMA_STREAM->CR |= DMA_SxCR_EN;				// Enable DMA transfer stream
	LCD_SPI->CR2 |= SPI_CR2_TXDMAEN;				// Enable SPI TX DMA
}


void LCD::DrawPixel(const uint16_t x, const uint16_t y, const RGBColour rgb)
{
	SetCursorPosition(x, y, x, y);

	Command(cmdILI9341::GRAM);
	Data16b(rgb.colour);
}


void LCD::DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const RGBColour colour)
{
	// Check lines are not too long
	if (x0 >= width)	x0 = width - 1;
	if (x1 >= width)	x1 = width - 1;
	if (y0 >= height)	y0 = height - 1;
	if (y1 >= height)	y1 = height - 1;
	if (y0 < 0)			y0 = 0;
	if (y1 < 0)			y1 = 0;

	// Flip co-ordinates if wrong way round
	if (x0 > x1) 		std::swap(x0, x1);
	if (y0 > y1) 		std::swap(y0, y1);

	int16_t dx = x1 - x0;
	int16_t dy = y1 - y0;

	// Vertical or horizontal line
	if (dx == 0 || dy == 0) {
		ColourFill(x0, y0, x1, y1, colour);
		return;
	}

	int16_t err = ((dx > dy) ? dx : -dy) / 2;

	while (1) {
		DrawPixel(x0, y0, colour);
		if (x0 == x1 && y0 == y1) {
			break;
		}

		// Work out whether to increment the x, y or both according to the slope
		if (err > -dx) {
			err -= dy;
			x0 += 1;
		}
		if (err < dy) {
			err += dx;
			y0 += 1;
		}
	}

}


void LCD::DrawRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const RGBColour rgb)
{
	// Check lines are not too long
	if (x0 >= width)	x0 = width - 1;
	if (x1 >= width)	x1 = width - 1;
	if (y0 >= height)	y0 = height - 1;
	if (y1 >= height)	y1 = height - 1;
	if (y0 < 0)			y0 = 0;
	if (y1 < 0)			y1 = 0;

	// Flip co-ordinates if wrong way round
	if (x0 > x1) 		std::swap(x0, x1);
	if (y0 > y1) 		std::swap(y0, y1);

	ColourFill(x0, y0, x1, y0, rgb);
	ColourFill(x0, y0, x0, y1, rgb);
	ColourFill(x0, y1, x1, y1, rgb);
	ColourFill(x1, y0, x1, y1, rgb);
}


void LCD::DrawChar(uint16_t x, uint16_t y, char c, const FontData& font, const RGBColour& foreground, const RGBColour background)
{
	// If at the end of a line of display, go to new line and set x to 0 position
	if ((x + font.Width) > width) {
		y += font.Height;
		x = 0;
	}

	// Write character colour data to array
	uint16_t px, py, i = 0;
	for (py = 0; py < font.Height; ++py) {
		const uint16_t fontRow = font.data[(c - 32) * font.Height + py];
		for (px = 0; px < font.Width; ++px) {
			if ((fontRow << (px)) & 0x8000) {			// for one byte characters use if ((fontRow << px) & 0x200) {
				charBuffer[currentCharBuffer][i] = foreground.colour;
			} else {
				charBuffer[currentCharBuffer][i] = background.colour;
			}
			++i;
		}
	}

	// Send array of data to SPI/DMA to draw
	while (SPI_DMA_Working);
	PatternFill(x, y, x + font.Width - 1, y + font.Height - 1, charBuffer[currentCharBuffer]);

	// alternate between the two character buffers so that the next character can be prepared whilst the last one is being copied to the LCD
	currentCharBuffer = currentCharBuffer == 1 ? 0 : 1;
}


// writes a character to an existing display array
void LCD::DrawCharMem(const uint16_t x, const uint16_t y, const uint16_t memWidth, uint16_t* memBuffer, char c, const FontData& font, const RGBColour foreground, const RGBColour background)
{
	// Write character colour data to array
	uint16_t px, py, i;

	for (py = 0; py < font.Height; py++) {
		i = (memWidth * (y + py)) + x;

		const uint16_t fontRow = font.data[(c - 32) * font.Height + py];
		for (px = 0; px < font.Width; ++px) {
			if ((fontRow << px) & 0x8000) {
				memBuffer[i] = foreground.colour;
			} else {
				memBuffer[i] = background.colour;
			}
			++i;
		}
	}
}


void LCD::DrawString(uint16_t x0, const uint16_t y0, const std::string_view s, const FontData& font, const RGBColour foreground, const RGBColour background)
{
	for (const char& c : s) {
		DrawChar(x0, y0, c, font, foreground, background);
		x0 += font.Width;
	}
}


void LCD::DrawStringCenter(uint32_t xCenter, const uint16_t y0, const size_t maxWidth, std::string_view s, const FontData& font, const uint16_t foreground, const uint16_t background)
{
	// If string width is less than width of draw buffer move x0 so that text will be centered
	const uint32_t strWidth = std::min(font.Width * s.length(), maxWidth);

	uint32_t x0 = std::max(xCenter - (strWidth / 2), 0UL);

	// Blank region before and after text if text width less than maximum space
	if (x0 > xCenter - (maxWidth / 2)) {
		ColourFill(xCenter - (maxWidth / 2), y0, x0, y0 + font.Height, background);
		ColourFill(xCenter + (strWidth / 2), y0, xCenter + (maxWidth / 2), y0 + font.Height, background);
	}

	for (const char& c : s) {
		DrawChar(x0, y0, c, font, foreground, background);
		x0 += font.Width;
		if (x0 > xCenter + (strWidth / 2) - font.Width) {
			break;
		}
	}
}


void LCD::DrawStringMem(uint16_t x0, const uint16_t y0, uint16_t const memWidth, uint16_t* memBuffer, std::string_view s, const FontData& font, const uint16_t foreground, const uint16_t background)
{
	for (const char& c : s) {
		DrawCharMem(x0, y0, memWidth, memBuffer, c, font, foreground, background);
		x0 += font.Width;
	}
}


void LCD::DrawStringMemCenter(uint16_t x0, const uint16_t y0, const size_t width, uint16_t* memBuffer, std::string_view s, const FontData& font, const uint16_t foreground, const uint16_t background)
{
	// If string width is less than width of draw buffer move x0 so that text will be centered
	const uint32_t strWidth = std::min(font.Width * s.length(), width);
	if (strWidth < width) {
		x0 = (width - strWidth) / 2;
	}

	for (const char& c : s) {
		DrawCharMem(x0, y0, width, memBuffer, c, font, foreground, background);
		x0 += font.Width;
		if (x0 > width - font.Width) {
			break;
		}
	}
}


void LCD::SPISetDataSize(const SPIDataSize_t& Mode)
{
	if (Mode == SPIDataSize_16b) {
		LCD_SPI->CR1 |= SPI_CR1_DFF;				// Data frame format: 0: 8-bit data frame format; 1: 16-bit data frame format
	} else {
		LCD_SPI->CR1 &= ~SPI_CR1_DFF;
	}
}


inline void LCD::SPISendByte(const uint8_t data)
{
	while (SPI_DMA_Working);

	// check if in 16 bit mode. Data frame format: 0: 8-bit data frame format; 1: 16-bit data frame format
	if (LCD_SPI->CR1 & SPI_CR1_DFF) {
		SPISetDataSize(SPIDataSize_8b);
	}

	uint8_t* SPI_DR = (uint8_t*)&(LCD_SPI->DR);		// cast the data register address as an 8 bit pointer - otherwise data gets packed wtih an extra byte of zeros
	*SPI_DR = data;

	while (SPI_DMA_Working);						// Wait for transmission to complete
}





