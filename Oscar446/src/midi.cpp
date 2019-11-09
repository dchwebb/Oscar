#include <midi.h>
#include <algorithm>

void MIDIHandler::ProcessMidi() {
	Timer += 1;

	if (QueueSize > 0) {
		bool edited = false;
		volatile uint8_t val1, val2;

		extern uint32_t MIDIDebug;
		MIDIDebug = QueueSize;

		MIDIType type = static_cast<MIDIType>(Queue[QueueRead] >> 4);
		uint8_t channel = Queue[QueueRead] & 0x0F;

		//NoteOn = 0x9, NoteOff = 0x8, PolyPressure = 0xA, ControlChange = 0xB, ProgramChange = 0xC, ChannelPressure = 0xD, PitchBend = 0xE, System = 0xF
		while ((QueueSize > 2 && (type == NoteOn || type == NoteOff || type == PolyPressure ||  type == ControlChange ||  type == PitchBend)) ||
				(QueueSize > 1 && (type == ProgramChange || type == ChannelPressure))) {

			QueueInc();
			val1 = Queue[QueueRead];
			QueueInc();
			if (type == ProgramChange || type == ChannelPressure) {
				val2 = 0;
			} else {
				val2 = Queue[QueueRead];
				QueueInc();
			}


			//	For more efficient display overwrite last event if the type is the same but the value (or note off) has changed
			if (!MIDIEvents.empty()) {

				auto event = MIDIEvents.begin();

				if (type == NoteOff && !(event->type == NoteOn && event->val1 == val1)) {
					event = std::find_if(MIDIEvents.begin(), MIDIEvents.end(), [&] (MIDIEvent me) { return me.type == NoteOn && me.val1 == val1 && me.channel == channel; } );
				}

				if (event->channel == channel && (type == event->type || (type == NoteOff && event->type == NoteOn)) &&
						((((type == NoteOff || type == ControlChange) && event->val1 == val1) || type == PitchBend || type == ChannelPressure))) {
					event->type = type;
					event->time = Timer;
					event->val1 = val1;
					event->val2 = val2;
					edited = true;
				}
			}

			if (!edited)
				MIDIEvents.push_front({Timer, type, channel, val1, val2});

			// erase first item if list greater than maximum size
			if (MIDIEvents.size() > 12)
				MIDIEvents.erase(MIDIEvents.end());

			type = static_cast<MIDIType>(Queue[QueueRead] >> 4);
			channel = Queue[QueueRead] & 0x0F;
		}

		// Clock
		if (QueueSize > 0 && Queue[QueueRead] == 0xF8) {
			ClockCount++;
			// MIDI clock triggers at 24 pulses per quarter note
			if (ClockCount == 6) {
				Clock = SysTickVal;
				ClockCount = 0;
			}
			QueueInc();
		}

		//	handle unknown data in queue
		if (QueueSize > 2 && type != 0x9 && type != 0x8 && type != 0xD && type != 0xE) {
			QueueInc();
		}
	}

	// Draw clock
	if (Clock > 0 && SysTickVal - Clock < 200) {
		lcd.ColourFill(300, 230, 305, 235, LCD_WHITE);
	} else {
		lcd.ColourFill(300, 230, 305, 235, LCD_BLACK);
	}

	if (QueueSize < 10) {
		// Draw midi events one at a time
		if (MIDIEvents.size() > 0) {
			if (MIDIPos >= MIDIEvents.size()) {
				MIDIPos = 0;
			}
			DrawEvent(MIDIEvents[MIDIPos]);
			MIDIPos++;
		}
	}
}


inline void MIDIHandler::QueueInc() {
	QueueSize--;
	QueueRead = (QueueRead + 1) % MIDIQUEUESIZE;
}

void MIDIHandler::DrawEvent(const MIDIEvent& event) {

	// Darken colour based on age
	uint16_t colour = ui.DarkenColour(MIDIColours[event.channel], (Timer - event.time) >> 6);

	volatile uint8_t top = MIDIDRAWHEIGHT * MIDIPos;

	lcd.DrawString(10, top, ui.intToString(event.channel + 1) + " ", &lcd.Font_Large, colour, LCD_BLACK);
	if (event.type == NoteOn || event.type == NoteOff) {


		//lcd.ColourFill(40, top + 1, 70, top + MIDIDRAWHEIGHT - 2, (event.type == NoteOn) ? colour : LCD_BLACK);
		if (event.type == NoteOn) {
			lcd.ColourFill(40, top + 1, 70, top + MIDIDRAWHEIGHT - 2, colour);
		} else {
			lcd.ColourFill(41, top + 2, 69, top + MIDIDRAWHEIGHT - 3, LCD_BLACK);
			lcd.DrawRect(40, top + 1, 70, top + MIDIDRAWHEIGHT - 2, colour);
		}

		lcd.DrawString(71, top, " " + NoteName(event.val1) + " ", &lcd.Font_Large, colour, LCD_BLACK);
		lcd.DrawString(115, top, " vel ", &lcd.Font_Large, colour, LCD_BLACK);
		lcd.DrawString(170, top, ui.intToString(event.val2) + "   ", &lcd.Font_Large, colour, LCD_BLACK);
	} else if (event.type == ChannelPressure) {
		lcd.DrawString(40, top, "Aftertouch ", &lcd.Font_Large, colour, LCD_BLACK);
		lcd.DrawString(170, top, ui.intToString(event.val1) + "  ", &lcd.Font_Large, colour, LCD_BLACK);
	} else if (event.type == PitchBend) {
		lcd.DrawString(40, top, "Pitchbend  ", &lcd.Font_Large, colour, LCD_BLACK);
		lcd.DrawString(170, top, ui.intToString(event.val1 + (event.val2 << 7)) + "   ", &lcd.Font_Large, colour, LCD_BLACK);
	} else if (event.type == ControlChange) {
		lcd.DrawString(40, top, "Control " + ui.intToString(event.val1) + "  ", &lcd.Font_Large, colour, LCD_BLACK);
		lcd.DrawString(170, top, ui.intToString(event.val2) + "    ", &lcd.Font_Large, colour, LCD_BLACK);
	}
}

const std::string pitches[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

std::string MIDIHandler::NoteName(const uint8_t& n) {
	uint8_t note = (n % 12);			// 67 = 'C'
	char octave = (n / 12) + 48;		// 48 = '0'
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
