#include <midi.h>
void MIDIHandler::ProcessMidi() {
	//lcd.DrawString(10, 10, "MIDI Mode", &lcd.Font_Small, LCD_WHITE, LCD_BLACK);
	if (MIDIQueue.size() > 0) {
		uint8_t msg = MIDIQueue.front() >> 4;
		uint8_t channel = MIDIQueue.front() & 0x0F;
		if ((msg == 0x9 || msg == 0x8) && MIDIQueue.size() > 2) {		// Note on/off

			MIDIQueue.pop();
			uint8_t note = MIDIQueue.front();
			MIDIQueue.pop();
			uint8_t velocity = MIDIQueue.front();
			MIDIQueue.pop();
			lcd.DrawString(10, 15 * MIDIPos, "Note " + (std::string)(msg == 0x9 ? "on " : "off") + " ch " + ui.intToString(channel + 1) + " " + NoteName(note) + " vel " + ui.intToString(velocity), &lcd.Font_Large, LCD_WHITE, LCD_BLACK);
			MIDIPos += 1;
		} else if (msg == 0xE && MIDIQueue.size() > 2) {		// Pitch bend
			MIDIQueue.pop();
			uint16_t pb = MIDIQueue.front();
			MIDIQueue.pop();
			pb += MIDIQueue.front() << 7;
			MIDIQueue.pop();
			lcd.DrawString(10, 15 * MIDIPos, "Pitch Bend  ch " + ui.intToString(channel + 1) + " " + ui.intToString(pb), &lcd.Font_Large, LCD_WHITE, LCD_BLACK);
			MIDIPos += 1;
		} else if (msg == 0xB && MIDIQueue.size() > 2) {		// Control change
			MIDIQueue.pop();
			uint16_t controller = MIDIQueue.front();
			MIDIQueue.pop();
			uint16_t val = MIDIQueue.front();
			MIDIQueue.pop();
			lcd.DrawString(10, 15 * MIDIPos, "Controller  ch " + ui.intToString(channel + 1) + " c " + ui.intToString(controller) + " v " + ui.intToString(val), &lcd.Font_Large, LCD_WHITE, LCD_BLACK);
			MIDIPos += 1;
		} else if (msg == 0xD && MIDIQueue.size() > 1) {		// Channel Pressure/Aftertouch
			MIDIQueue.pop();
			uint16_t amt = MIDIQueue.front();
			MIDIQueue.pop();
			lcd.DrawString(10, 15 * MIDIPos, "Aftertouch  ch " + ui.intToString(channel + 1) + " " + ui.intToString(amt), &lcd.Font_Large, LCD_WHITE, LCD_BLACK);
			MIDIPos += 1;
		}

		//	handle unknown data in queue
		if (MIDIQueue.size() > 2 && msg != 0x9 && msg != 0x8 && msg != 0xE) {
			MIDIQueue.pop();
		}

		if (MIDIPos > 13) {
			lcd.ScreenFill(LCD_BLACK);
			MIDIPos = 0;
		}
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
