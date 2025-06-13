#include <MidiEvents.h>
#include <ui.h>

MidiEvents midiEvents;

void MidiEvents::ProcessMidi()
{
	++timer;

	for (uint32_t source = 0; source < 2; ++source) {
		if (queueCount[source] > 0) {
			bool edited = false;
			uint8_t val1, val2;

			MIDIType type = static_cast<MIDIType>(queue[source][queueRead[source]] >> 4);
			uint8_t channel = queue[source][queueRead[source]] & 0x0F;

			//NoteOn = 0x9, NoteOff = 0x8, PolyPressure = 0xA, ControlChange = 0xB, ProgramChange = 0xC, ChannelPressure = 0xD, PitchBend = 0xE, System = 0xF
			while ((queueCount[source] > 2 && (type == NoteOn || type == NoteOff || type == PolyPressure || type == ControlChange || type == PitchBend)) ||
					(queueCount[source] > 1 && (type == ProgramChange || type == ChannelPressure))) {

				QueueInc(source);
				val1 = queue[source][queueRead[source]];
				QueueInc(source);
				if (type == ProgramChange || type == ChannelPressure) {
					val2 = 0;
				} else {
					val2 = queue[source][queueRead[source]];
					QueueInc(source);
				}


				//	For more efficient display overwrite last event if the type is the same but the value (or note off) has changed
				if (eventCount > 0) {
					// Handle note off
					if (type == NoteOff) {
						for (auto& event : midiEvents) {
							if (event.type == NoteOn && event.val1 == val1 && event.channel == channel && event.source == source) {
								event.type = NoteOff;
								event.time = timer;
								edited = true;
							}
						}

					} else {
						// Overwrite latest event in case of repeated control, pitchbend or aftertouch messages

						// If event from different source go to next in list in case events are interleaved from different sources
						uint32_t eventIndex = eventTail;
						if (midiEvents[eventTail].source != source) {
							--eventIndex;
							if (eventIndex > eventSize)	eventIndex = eventSize;		// uint will wrap to max int value
						}

						auto& event = midiEvents[eventIndex];

						if (event.channel == channel && event.source == source && type == event.type &&
								((type == ControlChange && event.val1 == val1) || type == PitchBend || type == ChannelPressure)) {
							event.type = type;
							event.time = timer;
							event.val1 = val1;
							event.val2 = val2;
							edited = true;
						}
					}
				}

				// MIDI event has not been used to update existing entry - add to circular buffer
				if (!edited) {
					if (++eventTail > eventSize - 1) {
						eventTail = 0;
					}
					if (eventCount < eventSize) {		// Once the circular buffer is full each new event will overwrite the oldest
						++eventCount;
					}
					midiEvents[eventTail] = {timer, type, channel, val1, val2, (QueueType)source};
				}

				type = static_cast<MIDIType>(queue[source][queueRead[source]] >> 4);
				channel = queue[source][queueRead[source]] & 0x0F;
			}

			// Clock
			while (queueCount[source] > 0 && queue[source][queueRead[source]] == 0xF8) {
				clockCount[source]++;
				// MIDI clock triggers at 24 pulses per quarter note
				if (clockCount[source] == 6) {
					clock[source] = SysTickVal;
					clockCount[source] = 0;
				}
				QueueInc(source);
				type = static_cast<MIDIType>(queue[source][queueRead[source]] >> 4);
			}

			//	handle unknown data in queue
			if (queueCount[source] > 2 && type != 0x9 && type != 0x8 && type != 0xD && type != 0xE) {
				QueueInc(source);
			}
		}

		// Draw clock
		const uint32_t offset = (source == QueueType::Serial) ? 300 : 20;
		if (clock[source] > 0 && SysTickVal - clock[source] < 50) {
			lcd.ColourFill(offset, 230, 5 + offset, 235, RGBColour::White);
		} else {
			lcd.ColourFill(offset, 230, 5 + offset, 235, RGBColour::Black);
		}

	}
	// Draw midi events one at a time, each time queue is processed
	if (queueCount[QueueType::Serial] + queueCount[QueueType::USB] < 10 && eventCount > 0) {
		if (drawIndex >= eventCount) {
			drawIndex = 0;
		}
		int32_t arrayIndex = eventTail - drawIndex;			// Tail is the newest item - work backwards to older entries
		if (arrayIndex < 0) {
			arrayIndex += eventSize;
		}
		DrawEvent(midiEvents[arrayIndex]);
		drawIndex++;
	}
}


inline void MidiEvents::QueueInc(uint32_t type)
{
	queueCount[type]--;
	if (++queueRead[type] == queueSize) queueRead[type] = 0;
}


void MidiEvents::QueueAdd(QueueType source, uint8_t data)
{
	// Adds a byte to the MIDI event queue
	queue[source][queueWrite[source]] = data;
	queueCount[source]++;
	if (++queueWrite[source] == queueSize) queueWrite[source] = 0;
}


void MidiEvents::DrawEvent(const MIDIEvent& event)
{
	// Darken colour based on age of event, limiting darkening effect so very old items just remain visible
	const RGBColour colour = RGBColour(MIDIColours[event.channel]).DarkenColour(std::min((timer - event.time) >> 7, (uint64_t)50));

	const uint8_t top = 5 + drawHeight * drawIndex;

	lcd.DrawString(10, top, ui.IntToString(event.channel + 1) + " ", lcd.Font_Large, colour, RGBColour::Black);		// Print channel number
	lcd.DrawString(260, top, event.source == QueueType::USB ? "USB" : "   ", lcd.Font_Large, colour, RGBColour::Black);		// Print event source

	if (event.type == NoteOn || event.type == NoteOff) {
		// Draw rectangle - filled if note is still sounding
		if (event.type == NoteOn) {
			lcd.ColourFill(40, top + 1, 70, top + drawHeight - 2, colour);
		} else {
			lcd.ColourFill(41, top + 2, 69, top + drawHeight - 3, RGBColour::Black);
			lcd.DrawRect(40, top + 1, 70, top + drawHeight - 2, colour);
		}

		lcd.DrawString(71, top, " " + NoteName(event.val1) + " ", lcd.Font_Large, colour, RGBColour::Black);
		lcd.DrawString(115, top, " vel ", lcd.Font_Large, colour, RGBColour::Black);
		lcd.DrawString(170, top, ui.IntToString(event.val2) + "   ", lcd.Font_Large, colour, RGBColour::Black);

	} else if (event.type == ChannelPressure) {
		lcd.DrawString(40, top, "Aftertouch ", lcd.Font_Large, colour, RGBColour::Black);
		lcd.DrawString(170, top, ui.IntToString(event.val1) + "  ", lcd.Font_Large, colour, RGBColour::Black);

	} else if (event.type == PitchBend) {
		lcd.DrawString(40, top, "Pitchbend  ", lcd.Font_Large, colour, RGBColour::Black);
		lcd.DrawString(170, top, ui.IntToString(event.val1 + (event.val2 << 7)) + "   ", lcd.Font_Large, colour, RGBColour::Black);

	} else if (event.type == ControlChange) {
		lcd.DrawString(40, top, "Control " + ui.IntToString(event.val1) + "  ", lcd.Font_Large, colour, RGBColour::Black);
		lcd.DrawString(170, top, ui.IntToString(event.val2) + "    ", lcd.Font_Large, colour, RGBColour::Black);
	}
}


std::string MidiEvents::NoteName(const uint8_t n)
{
	static const std::string pitches[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

	const uint8_t note = (n % 12);			// 67 = 'C'
	const char octave = (n / 12) + 48;		// 48 = '0'
	return pitches[note] + std::string(1, octave);

}

/*
Message Type	MS Nybble	LS Nybble		Bytes		Data Byte 1			Data Byte 2
-----------------------------------------------------------------------------------------
Note Off		0x8			Channel			2			Note Number			Velocity
Note On			0x9			Channel			2			Note Number			Velocity
Poly Pressure	0xA			Channel			2			Note Number			Pressure
Control Change	0xB			Channel			2			Controller 			Value
Program Change	0xC			Channel			1			Program Number		-none-
Ch. Pressure	0xD			Channel			1			Pressure			-none-
Pitch Bend		0xE			Channel			2			Bend LSB (7-bit)	Bend MSB (7-bits)
System			0xF			further spec	variable	variable			variable

*/
