//  Source file for the music utility routines.
//  Version 0.0.  DAJP - 2nd Nov 2026.
#include "my_music_utils.h"

// To prevent time-consuming calculations later, I'll store the
// frequencies for the octave starting at middle C (midi note 60).
// All other octaves are easy to calculate from here.
const float scaleFrequencies[] = {261.63, 277.18, 293.67, 311.13,
		329.63, 349.23, 369.99, 392.00,
		415.30, 440.00, 466.16, 493.88, 523.25};

// The chordNotes array contains offsets to six notes
// in the chord (data from the conductor protocol spec.).
// To enable arpeggios, notes are repeated if necessary.
const uint8_t chordNotes[19][6] = {
		{0, 0, 0, 0, 0, 0},    // Chord type zero is not used
		{0, 4, 7, 12, 16, 19}, // Standard major chord
		{0, 3, 7, 12, 15, 19}, // Standard minor chord
		{0, 3, 6, 12, 15, 18}, // Diminished
		{0, 3, 6, 9, 12, 15},  // Diminished seventh
		{0, 5, 7, 12, 17, 19}, // Suspended fourth
		{0, 4, 7, 9, 12, 16},  // Sixth
		{0, 4, 7, 10, 12, 19}, // Seventh
		{0, 4, 7, 11, 12, 19}, // Major seventh
		{0, 4, 7, 10, 14, 19}, // Ninth
		{0, 4, 7, 12, 14, 19}, // Add ninth
		{0, 7, 12, 19, 24, 31}, // Bare fifth
		{0, 5, 7, 10, 12, 19}, // Suspended fourth seventh
		{0, 4, 7, 10, 12, 19}, // Minor seventh
		{0, 4, 7, 10, 14, 16}, // Minor ninth
		{0, 2, 7, 12, 14, 19}, // Suspended second
		{0, 4, 6, 10, 12, 16}, // Minor seventh flattened fifth
		{0, 4, 7, 9, 12, 16},  // Minor sixth
		{0, 12, 0, 12, 0, 12}  // Tonic only
};

// This function provides the frequency of any note asked.  It takes in the
// MIDI code of that note:
float getFreqOfMidiNote(uint8_t midiNote) {
	float factor = 1.0;
	while (midiNote < 60) {
		midiNote += 12;
		factor *= 0.5;
	}
	while (midiNote > 72) {
		midiNote -= 12;
		factor *= 2.0;
	}
	return scaleFrequencies[midiNote - 60] * factor;
}

// Then another one that takes in a chord root and type, and
// returns a series of notes in that chord by MIDI note value:
void getChordNotes(uint8_t *notes, uint8_t chordRoot, uint8_t chordType, uint16_t howManyNotes) {
	uint8_t const *chord = chordNotes[chordType];
	uint8_t noteCounter = 0;
	while (howManyNotes-- > 0) {
		notes[noteCounter] = chordRoot + chord[noteCounter % 6];
		noteCounter++;
	}
}
