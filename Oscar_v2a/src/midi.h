#pragma once

#include "initialisation.h"
#include <lcd.h>


class MIDIHandler {
public:
	void ProcessMidi();

	//	Variables used by interrupt routine to insert raw MIDI events into processing queue
	static constexpr uint8_t queueSize = 200;
	uint8_t queue[queueSize];		// Raw MIDI events inserted by interrupt handler
	uint8_t queueRead = 0;			// Used by ProcessMidi() to read out events from queue
	uint8_t queueWrite = 0;			// Used by interrupt handler to add event at next position in queue
	uint8_t queueCount = 0;			// Incremented each time MIDI interrupt fires; decremented in ProcessMidi()
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
	static constexpr uint32_t eventSize = 12;			// Number of events that can be displayed in list in UI

	std::array<MIDIEvent, eventSize> midiEvents;		// Circular buffer to hold processed MIDI events
	uint8_t eventTail = eventSize;
	uint8_t eventCount = 0;								// Number of MIDI events in list - will max out at eventSize (12)

	uint8_t drawIndex = 0;								// Index of current MIDI event being drawn
	uint64_t timer;
	uint64_t clock;
	uint8_t clockCount = 0;								// Used to show flashing dot indicating clock timing
	const uint16_t MIDIColours[16] = {RGBColour::White, RGBColour::Red, RGBColour::Green, RGBColour::Blue, RGBColour::LightBlue, RGBColour::Yellow, RGBColour::Orange, RGBColour::Cyan, RGBColour::Magenta, RGBColour::Brown, RGBColour::White, RGBColour::Red, RGBColour::Green, RGBColour::Blue, RGBColour::LightBlue, RGBColour::Yellow};

	void DrawEvent(const MIDIEvent& event);
	void QueueInc();
	std::string NoteName(const uint8_t note);
};

extern MIDIHandler midi;
