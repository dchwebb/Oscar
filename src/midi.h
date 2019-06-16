#pragma once

#include <queue>
#include <deque>
#include <lcd.h>
#include <ui.h>

#define MIDIDRAWHEIGHT 17
extern LCD lcd;

enum MIDIType {NoteOn = 0x9, NoteOff = 0x8, PolyPressure = 0xA, ControlChange = 0xB, ProgramChange = 0xC, ChannelPressure = 0xD, PitchBend = 0xE, System = 0xF };

struct MIDIEvent {
	uint64_t time;
	MIDIType type;
	uint8_t channel;
	uint8_t val1;
	uint8_t val2;
};

class MIDIHandler {
public:
	void ProcessMidi();
	std::queue<uint8_t> MIDIQueue;
private:
	uint8_t MIDIPos = 0;
	std::string NoteName(const uint8_t& note);
	std::deque<MIDIEvent> MIDIEvents;
	uint64_t Timer;
	void DrawEvent(const MIDIEvent& event);
	const uint16_t MIDIColours[16] = {LCD_WHITE, LCD_RED, LCD_GREEN, LCD_BLUE, LCD_LIGHTBLUE, LCD_YELLOW, LCD_ORANGE, LCD_CYAN, LCD_MAGENTA, LCD_BROWN, LCD_WHITE, LCD_RED, LCD_GREEN, LCD_BLUE, LCD_LIGHTBLUE, LCD_YELLOW};
};
