#pragma once

#include "initialisation.h"
#include <lcd.h>
#include <ui.h>

#define MIDIDRAWHEIGHT 17
#define MIDIQUEUESIZE 20
extern LCD lcd;

enum MIDIType {Unknown = 0, NoteOn = 0x9, NoteOff = 0x8, PolyPressure = 0xA, ControlChange = 0xB, ProgramChange = 0xC, ChannelPressure = 0xD, PitchBend = 0xE, System = 0xF };

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
	//std::queue<uint8_t> MIDIQueue;
	uint8_t Queue[MIDIQUEUESIZE];
	uint8_t QueueRead = 0;
	uint8_t QueueWrite = 0;
	uint8_t QueueSize = 0;
private:
	const uint16_t MIDIColours[16] = {LCD_WHITE, LCD_RED, LCD_GREEN, LCD_BLUE, LCD_LIGHTBLUE, LCD_YELLOW, LCD_ORANGE, LCD_CYAN, LCD_MAGENTA, LCD_BROWN, LCD_WHITE, LCD_RED, LCD_GREEN, LCD_BLUE, LCD_LIGHTBLUE, LCD_YELLOW};
	std::deque<MIDIEvent> MIDIEvents;
	uint8_t MIDIPos = 0;
	uint64_t Timer;
	uint64_t Clock;
	uint8_t ClockCount = 0;
	void DrawEvent(const MIDIEvent& event);
	void QueueInc();
	std::string NoteName(const uint8_t& note);
};
