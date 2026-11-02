//  Header file for the my_music_utils routines.
//  Version 0.0.  DAJP - 2nd Nov 2026

#ifndef __MY_MUSIC_UTILS_H
#define __MY_MUSIC_UTILS_H

#include <stdbool.h>
#include <stdint.h>
#include <math.h>

// There's a function here that returns the frequency of any note
// asked, it takes in the MIDI note number:
float getFreqOfMidiNote(uint8_t midiNote);

// Then another one that takes in a chord root and type, and
// fills up the notes array with a series of notes in that chord.
void getChordNotes(uint8_t *notes, uint8_t chordRoot, uint8_t chordType, uint16_t howManyNotes);

#endif /* __MY_MUSIC_UTILS_H */
