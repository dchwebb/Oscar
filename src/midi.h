#pragma once

#include <queue>
#include <lcd.h>
#include <ui.h>

//extern std::queue<uint8_t> MIDIQueue;
extern LCD lcd;

class MIDIHandler {
public:
	void ProcessMidi();
	std::queue<uint8_t> MIDIQueue;
private:
	uint8_t MIDIPos = 0;
	std::string NoteName(const uint8_t& note);
};
