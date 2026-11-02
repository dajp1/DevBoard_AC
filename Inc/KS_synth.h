//  Header file for the FM synthesis routines.
//  Version 0.0.  DAJP - 24th Oct 2026

#ifndef __KS_SYNTH_H
#define __KS_SYNTH_H

#include <stdbool.h>
#include <stdint.h>
#include <math.h>

// Notes are identified by a status eum, a startTime (relative
// to the HAL_GetTime() in this STM32), and the offsets and steps
// into the sine wave lookup table and the envelope lookup tables.

#include "FM_synth.h" // For the definition of enum noteStatus

struct KS_noteType {
	enum noteStatus status;
	uint32_t startTimeInMilliSeconds;
	uint32_t samplesSinceStart;
	float mainPhaseOffset, mainPhaseStep;
	float amplitude;
	uint8_t KSBufferNumber;
};

// I will allow up to eight notes to be stored for future play,
// but only four will be able to be played at the same time.
// They will be assigned to the KS_notes[] array in round-robin
// fashion, so any attempt to queue up more than eight notes
// will result in notes queued earlier being deleted.  The
// four notes being played will be those with start times
// before, but closest to the current time.
#define KS_BUF_SIZE 256
#define MAX_KS_NOTES 8
#define NUM_KS_BUFFERS 8

// Functions include queueing a new note, and adding the current
// FM synthesis contributions to the audio buffer:
void setSampleRateKS(uint32_t newRate);
void initKSBuffersAndNotes();
void addNewKSNote(uint32_t when, float frequency, float amplitude);
void addKSsamples(uint16_t *buffer, uint32_t start, uint32_t end);
void SetKSDampingFactors(float filterFactor, float decayFactor);

#endif /* __KS_SYNTH_H */


