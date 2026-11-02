//  Header file for the FM synthesis routines.
//  Version 0.0.  DAJP - 24th Oct 2026

#ifndef __FM_SYNTH_H
#define __FM_SYNTH_H

#include <stdbool.h>
#include <stdint.h>
#include <math.h>

// Notes are identified by a status enum, a startTime (relative
// to the HAL_GetTime() in this STM32), and the offsets and steps
// into the sine wave lookup table and the envelope lookup tables.

enum noteStatus {Off, Pending, Active};

struct FM_noteType {
	enum noteStatus status;
	uint32_t startTimeInMilliSeconds;
	uint32_t samplesSinceStart;
	float mainPhaseOffset, mainPhaseStep;
	float modPhaseOffset, modPhaseStep;
	float lastMainEnv, lastModEnv;
};

// FM synthesis requires a sine wave to start with.  Note that SINE_BUF_SIZE
// must be a power of two (otherwise the time-saving trick of ensuring that
// all accesses are within the buffer doesn't work).
#define SINE_BUF_SIZE 512
#define SINE_BUF_SIZE_MINUS_ONE 511
extern float sineBuffer[SINE_BUF_SIZE];

// The envelopes for the FM sounds are defined as arrays of floats
// A, D, SL, ST, R, where A, D, ST and R are times in terms of
// milliseconds, and SL is the sustain level from zero to one.
void setEnvelope(bool bIsMain, float attackInMs, float decayInMs, float sustainLevel, float sustainInMs, float releaseInMs);

// You can also set the modulation factor and frequency; the
// frequency is a factor of the frequency of the note being
// played.
void setModulation(float modFactor, float freqMultiplier);

// I will allow up to eight notes to be stored for future play,
// but only three will be able to be played at the same time.
// They will be assigned to the FM_notes[] array in round-robin
// fashion, so any attempt to queue up more than eight notes
// will result in notes queued earlier being deleted.  The
// three notes being played will be those with start times
// before, but closest to the current time.
#define MAX_FM_NOTES 8

// Functions include queueing a new note, and adding the current
// FM synthesis contributions to the audio buffer:
void setSampleRateFM(uint32_t newRate);
void fillSineArray();
void addNewFMNote(uint32_t when_ms, float frequency);
void addFMsamples(uint16_t *buffer, uint32_t start, uint32_t end);

#endif /* __FM_SYNTH_H */
