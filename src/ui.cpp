#include <ui.h>

std::string ui::floatToString(float f) {
	std::stringstream ss;
	ss << (int16_t)std::round(f * 10);
	std::string s = ss.str();
	s.insert(s.length() - 1, ".");
	return s;
}
