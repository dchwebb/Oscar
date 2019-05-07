#include <ui.h>

void ui::DrawUI() {
	// Draw UI
	lcd.ColourFill(0, DRAWHEIGHT + 1, DRAWWIDTH - 1, DRAWHEIGHT + 1, LCD_GREY);
	lcd.ColourFill(0, 239, DRAWWIDTH - 1, 239, LCD_GREY);
	lcd.ColourFill(0, DRAWHEIGHT + 1, 0, 239, LCD_GREY);
	lcd.ColourFill(90, DRAWHEIGHT + 1, 90, 239, LCD_GREY);
	lcd.ColourFill(230, DRAWHEIGHT + 1, 230, 239, LCD_GREY);
	lcd.ColourFill(319, DRAWHEIGHT + 1, 319, 239, LCD_GREY);

	lcd.DrawString(10, DRAWHEIGHT + 8, "Zoom Horiz", &lcd.Font_Small, LCD_GREY, LCD_BLACK);
	lcd.DrawString(240, DRAWHEIGHT + 8, "Zoom Vert", &lcd.Font_Small, LCD_GREY, LCD_BLACK);

	uint16_t screenMs = std::round(320000.0f * TIM3->PSC * TIM3->ARR / SystemCoreClock);
	std::stringstream ss;
	ss << screenMs << "ms    ";
	std::string s = ss.str();

	s = floatToString(640000.0f * (TIM3->PSC + 1) * (TIM3->ARR + 1) / SystemCoreClock) + "ms    ";
	lcd.DrawString(140, DRAWHEIGHT + 8, s, &lcd.Font_Small, LCD_WHITE, LCD_BLACK);

}

std::string ui::floatToString(float f) {
	std::stringstream ss;
	ss << (int16_t)std::round(f * 10);
	std::string s = ss.str();
	s.insert(s.length() - 1, ".");
	return s;
}

std::string ui::intToString(uint16_t v) {
	std::stringstream ss;
	ss << v;
	return ss.str();
}

