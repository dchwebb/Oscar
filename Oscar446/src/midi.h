#pragma once

#include "initialisation.h"
#include <lcd.h>
#include <ui.h>

extern LCD lcd;


class MIDIHandler {
public:
	void ProcessMidi();

	//	Variables used by interrupt routine to insert raw MIDI events into processing queue
	static constexpr uint8_t queueSize = 100;
	uint8_t queue[queueSize];
	uint8_t queueRead = 0;
	uint8_t queueWrite = 0;
	uint8_t queueCount = 0;
private:
	enum MIDIType {Unknown = 0, NoteOn = 0x9, NoteOff = 0x8, PolyPressure = 0xA, ControlChange = 0xB, ProgramChange = 0xC,
		ChannelPressure = 0xD, PitchBend = 0xE, System = 0xF };

	struct MIDIEvent {
		uint64_t time;
		MIDIType type;
		uint8_t channel;
		uint8_t val1;
		uint8_t val2;
	};

	static constexpr uint8_t drawHeight = 17;
	static constexpr uint32_t eventSize = 12;

	std::array<MIDIEvent, eventSize> midiEvents;		// Circular buffer to hold processed MIDI events
	uint8_t eventTail = eventSize;
	uint8_t eventCount = 0;

	uint8_t drawIndex = 0;								// Index of current MIDI event being drawn
	uint64_t timer;
	uint64_t clock;
	uint8_t clockCount = 0;								// Used to show flashing dot indicating clock timing
	const uint16_t MIDIColours[16] = {LCD_WHITE, LCD_RED, LCD_GREEN, LCD_BLUE, LCD_LIGHTBLUE, LCD_YELLOW, LCD_ORANGE, LCD_CYAN, LCD_MAGENTA, LCD_BROWN, LCD_WHITE, LCD_RED, LCD_GREEN, LCD_BLUE, LCD_LIGHTBLUE, LCD_YELLOW};

	void DrawEvent(const MIDIEvent& event);
	void QueueInc();
	std::string NoteName(const uint8_t note);
};
