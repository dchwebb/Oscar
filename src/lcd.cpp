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
	CommandData(CDARGS {ILI9341_DTCA, 0x85, 0x00, 0x78});		// default is  0x85, 0x00, 0x78
	CommandData(CDARGS {ILI9341_DTCB, 0x00,	0x00});				// default is  0x66, 0x00
	CommandData(CDARGS {ILI9341_POWER_SEQ, 0x64, 0x03, 0x12, 0x81});		// default is 0x55 0x01 0x23 0x01
	CommandData(CDARGS {ILI9341_PRC, 0x20});					// 0x20: DDVDH = 2xVCI  (Pump ratio control)
	CommandData(CDARGS {ILI9341_POWER1,	0x23});					// 0x23: GVDD level = 4.6V (reference level for VCOM level and grayscale voltage level)
	CommandData(CDARGS {ILI9341_POWER2,	0x10});					// Appears to be invalid - should be 0b000 - 0b011
	CommandData(CDARGS {ILI9341_VCOM1, 0x3E, 0x28});			// 0x3E: VCOMH = 4.250V; 0x28: VCOML = 3.700V
	CommandData(CDARGS {ILI9341_VCOM2, 0x86});					// 0x86: VCOM offset voltage = VMH - 58 VML – 58;
	CommandData(CDARGS {ILI9341_MAC, 0x48});					// Memory access control: MX = Column Address Order, RGB-BGR Order control
	CommandData(CDARGS {ILI9341_PIXEL_FORMAT, 0x55});			// 16 bit format
	CommandData(CDARGS {ILI9341_FRC, 0x00, 0x18});
	CommandData(CDARGS {ILI9341_DFC, 0x08, 0x82, 0x27});
	CommandData(CDARGS {ILI9341_3GAMMA_EN, 0x00});
	CommandData(CDARGS {ILI9341_COLUMN_ADDR, 0x00, 0x00, 0x00, 0xEF});
	CommandData(CDARGS {ILI9341_PAGE_ADDR, 0x00, 0x00, 0x01, 0x3F});
	CommandData(CDARGS {ILI9341_GAMMA, 0x01});
	CommandData(CDARGS {ILI9341_PGAMMA,	0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00});
	CommandData(CDARGS {ILI9341_NGAMMA, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F});
	Command(ILI9341_SLEEP_MODE);

	Delay(1000000);

	Command(ILI9341_DISPLAY_ON);
	Command(ILI9341_GRAM);
};

void Lcd::CommandData(CDARGS cmds) {
	Command(cmds[0]);

	LCD_DCX_SET;
	LCD_CS_RESET;

	for (uint8_t i = 1; i < cmds.size(); ++i)
		SPISendByte(cmds[i]);

	LCD_CS_SET;
}

void Lcd::Delay(volatile uint32_t delay) {
	for (; delay != 0; delay--);
}

void Lcd::Command(uint8_t data) {
	LCD_DCX_RESET;
	LCD_CS_RESET;
	SPISendByte(data);
	LCD_CS_SET;
}

//	Send data - use overloads to allow 16 and 32 bit sends
void Lcd::Data(uint8_t data) {
	LCD_DCX_SET;
	LCD_CS_RESET;
	SPISendByte(data);
	LCD_CS_SET;
}

void Lcd::Data16b(uint16_t data) {
	LCD_DCX_SET;
	LCD_CS_RESET;
	SPISendByte(data >> 8);
	SPISendByte(data & 0xFF);
	LCD_CS_SET;
}

void Lcd::SetCursorPosition(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
	Command(ILI9341_COLUMN_ADDR);
	Data16b(x1);
	Data16b(x2);

	Command(ILI9341_PAGE_ADDR);
	Data16b(y1);
	Data16b(y2);

}


void Lcd::Rotate(LCD_Orientation_t o) {
	Command(ILI9341_MAC);
	switch (o) {
		case LCD_Portrait :				Data(0x58);
		case LCD_Portrait_Flipped : 	Data(0x88);
		case LCD_Landscape : 			Data(0x28);
		case LCD_Landscape_Flipped :	Data(0xE8);
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

void Lcd::ScreenFill(const uint16_t& colour) {
	ColourFill(0, 0, width - 1, height - 1, colour);
}

void Lcd::ColourFill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const uint16_t& colour) {

	SetCursorPosition(x0, y0, x1, y1);

	Command(ILI9341_GRAM);

	uint32_t pixelCount = (x1 - x0 + 1) * (y1 - y0 + 1);

	LCD_CS_RESET;
	LCD_DCX_SET;

	SPISetDataSize(SPIDataSize_16b);			// 16-bit SPI mode

	// Send first 65535 bytes, SPI must be in 16-bit Mode
	SPI_DMA_SendHalfWord(colour, (pixelCount > 0xFFFF) ? 0xFFFF : pixelCount);

	while (SPI_DMA_Working);

	// Check if there is more data to send
	if (pixelCount > 0xFFFF) {
		SPI_DMA_SendHalfWord(colour, pixelCount - 0xFFFF);		// Send remaining data

		while (SPI_DMA_Working);
	}

	LCD_CS_SET;

	SPISetDataSize(SPIDataSize_8b);				// 8 bit SPI Mode
}


void Lcd::PatternFill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const uint16_t* PixelData) {

	SetCursorPosition(x0, y0, x1, y1);
	Command(ILI9341_GRAM);
	uint32_t pixelCount = (x1 - x0 + 1) * (y1 - y0 + 1);

	LCD_CS_RESET;
	LCD_DCX_SET;

	SPISetDataSize(SPIDataSize_16b);			// 16-bit SPI mode

	if (DMA2_Stream6->NDTR)							// Check number of data items to transfer is zero (ie stream is free)
		return;

	// Clear DMA Stream 6 flags using high interrupt flag clear register
	DMA2->HIFCR = DMA_HIFCR_CFEIF6 | DMA_HIFCR_CDMEIF6 | DMA_HIFCR_CTEIF6 | DMA_HIFCR_CHTIF6 | DMA_HIFCR_CTCIF6;

	DMA2_Stream6->CR |= DMA_SxCR_MINC;				// Memory in increment mode
	DMA2_Stream6->NDTR = pixelCount;				// Number of data items to transfer
	DMA2_Stream6->M0AR = (uint32_t) (PixelData);	// DMA_InitStruct.DMA_Memory0BaseAddr
	DMA2_Stream6->CR |= DMA_SxCR_EN;				// Enable DMA transfer stream
	SPI5->CR2 |= SPI_CR2_TXDMAEN;					// Enable SPI TX DMA

	while (SPI_DMA_Working);

	LCD_CS_SET;
	SPISetDataSize(SPIDataSize_8b);				// 8 bit SPI Mode
}

void Lcd::DrawPixel(uint16_t x, uint16_t y, const uint16_t& colour) {
	SetCursorPosition(x, y, x, y);

	Command(ILI9341_GRAM);
	Data16b(colour);

}


void Lcd::DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const uint32_t& colour) {

	int16_t dx, dy, err;
	uint16_t tmp;

	// Check lines are not too long
	if (x0 >= width)	x0 = width - 1;
	if (x1 >= width)	x1 = width - 1;
	if (y0 >= height)	y0 = height - 1;
	if (y1 >= height)	y1 = height - 1;


	// Flip co-ordinates if wrong way round
	if (x0 > x1) {
		tmp = x0; x0 = x1; x1 = tmp;
	}
	if (y0 > y1) {
		tmp = y0; y0 = y1; y1 = tmp;
	}

	dx = x1 - x0;
	dy = y1 - y0;

	// Vertical or horizontal line
	if (dx == 0 || dy == 0) {
		ColourFill(x0, y0, x1, y1, colour);
		return;
	}

	err = ((dx > dy) ? dx : -dy) / 2;

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


void Lcd::DrawChar(uint16_t x, uint16_t y, char c, const FontDef_t *font, const uint32_t& foreground, const uint32_t& background) {


	// If at the end of a line of display, go to new line and set x to 0 position
	if ((x + font->Width) > width) {
		y += font->Height;
		x = 0;
	}

	// Create array containing colour pattern to display
	uint16_t charDisp[font->Width * font->Height];

	// Write character colour data to array
	uint16_t px, py, fontRow, i = 0;
	for (py = 0; py < font->Height; py++) {
		fontRow = font->data[(c - 32) * font->Height + py];
		for (px = 0; px < font->Width; px++) {
			if ((fontRow << px) & 0x8000) {
				charDisp[i] = foreground;
			} else {
				charDisp[i] = background;
			}
			i++;
		}
	}

	// Send array of data to SPI/DMA to draw
	PatternFill(x, y, x + font->Width - 1, y + font->Height - 1, charDisp);
}

void Lcd::DrawString(uint16_t x0, uint16_t y0, std::string s, const FontDef_t *font, const uint32_t& foreground, const uint32_t& background) {
	for (char& c : s) {
		DrawChar(x0, y0, c, font, foreground, background);
		x0 += font->Width;
	}
}


/*void Lcd::DrawChar(uint16_t x, uint16_t y, char c, const FontDef_t *font, const uint32_t& foreground, const uint32_t& background) {
	uint32_t i, b, j;
	// Set character coordinates
	charPosX = x;
	charPosY = y;

	if ((charPosX + font->Width) > width) {
		 If at the end of a line of display, go to new line and set x to 0 position
		charPosY += font->Height;
		charPosX = 0;
	}

	// Draw rectangle for background
	ColourFill(charPosX, charPosY, charPosX + font->Width, charPosY + font->Height, background);

	// Draw font data
	for (i = 0; i < font->Height; i++) {
		b = font->data[(c - 32) * font->Height + i];
		for (j = 0; j < font->Width; j++) {
			if ((b << j) & 0x8000) {
				DrawPixel(charPosX + j, (charPosY + i), foreground);
			}
		}
	}

	// Set new character position
	charPosX += font->Width;
}*/

void Lcd::SPISetDataSize(SPIDataSize_t Mode) {

	SPI5->CR1 &= ~SPI_CR1_SPE;						// Disable SPI

	if (Mode == SPIDataSize_16b) {
		SPI5->CR1 |= SPI_CR1_DFF;					// Data frame format: 0: 8-bit data frame format; 1: 16-bit data frame format
	} else {
		SPI5->CR1 &= ~SPI_CR1_DFF;
	}

	SPI5->CR1 |= SPI_CR1_SPE;						// Re-enable SPI
}

inline void Lcd::SPISendByte(uint8_t data) {
//	while (SPI_Working);	// Wait for previous transmissions to complete if DMA TX enabled for SPI

	SPI5->DR = data;		// Fill output buffer with data

	while (SPI_Working);	// Wait for transmission to complete
}

bool Lcd::SPI_DMA_SendHalfWord(uint16_t value, uint16_t count) {

	if (DMA2_Stream6->NDTR)							// Check number of data items to transfer is zero (ie stream is free)
		return false;

	DMAint16 = value;								// data to transfer - use class property so does not go out of scope

	// Clear DMA Stream 6 flags using high interrupt flag clear register
	DMA2->HIFCR = DMA_HIFCR_CFEIF6 | DMA_HIFCR_CDMEIF6 | DMA_HIFCR_CTEIF6 | DMA_HIFCR_CHTIF6 | DMA_HIFCR_CTCIF6;

	DMA2_Stream6->CR &= ~DMA_SxCR_MINC;				// Memory not in increment mode
	DMA2_Stream6->NDTR = count;						// Number of data items to transfer
	DMA2_Stream6->M0AR = (uint32_t) &DMAint16;		// DMA_InitStruct.DMA_Memory0BaseAddr;
	DMA2_Stream6->CR |= DMA_SxCR_EN;				// Enable DMA transfer stream

	SPI5->CR2 |= SPI_CR2_TXDMAEN;					// Enable SPI TX DMA

	return true;
}



