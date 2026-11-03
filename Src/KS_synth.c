//  Source file for the FM synthesis routines.
//  Version 0.0.  DAJP - 24th Oct 2026.
#include "KS_synth.h"
#include "stm32g4xx_hal.h"

// Some variables required for these functions.  I will assign four buffers
// for a maximum of four-note polyphony:
float KS_Buffers[NUM_KS_BUFFERS][KS_BUF_SIZE];

// Notes will be assigned sequentially in the KS_notes array:
struct KS_noteType KS_notes[MAX_KS_NOTES];
uint16_t nextIndexNumber = 0;

// The routines will need to know the sample rate:
static uint32_t sampleRate = 44100;
void setSampleRateKS(uint32_t newRate) { sampleRate = newRate; }

// The damping factor can be set externally.  I'll store both the damping
// factor and (1 - damping factor) to avoid having to do the subtractions
// for every sample.  Decay factor should be large for a slow decay
// (try 1000 or more); damping factor controls the rate at which the
// waveform changes.  Default values are for (0.5, 2000).
float damping_factor = 0.4995;
float one_minus_damping_factor = 0.4995;
void SetKSDampingFactor(float filterFactor, float decayFactor) {
	damping_factor = filterFactor * (1 - 1/decayFactor);
	one_minus_damping_factor = (1 - filterFactor) * (1 - 1/decayFactor);
}

// Zero all the arrays and clear all the notes to initialise them:
void initKSBuffersAndNotes() {
	// First initialise the buffers to be full of all zeros:
	for (uint32_t buffer = 0; buffer < NUM_KS_BUFFERS; buffer++){
		for (uint32_t loop = 0; loop < KS_BUF_SIZE; loop++) {
			KS_Buffers[buffer][loop] = 0.0;
		}
	}
	// Then initialise the notes to be off, with default settings:
	for (uint32_t loop = 0; loop < MAX_KS_NOTES; loop++) {
		KS_notes[loop].status = Off;
		KS_notes[loop].KSBufferNumber = 0;
		KS_notes[loop].mainPhaseOffset = 0;
		KS_notes[loop].mainPhaseStep = 0;
		KS_notes[loop].samplesSinceStart = 0;
		KS_notes[loop].startTimeInMilliSeconds = 0x7FFFFFFF;
	}
}

// Fill up the KS buffer with a square wave shape that will eventually
// average out to zero:
void FillKSBuffer(float *buf, float amp) {
	uint16_t loop = 0;
	for ( ; loop < KS_BUF_SIZE / 4; loop++) {
		buf[loop] = 1000.0 * amp;
	}
	for ( ; loop < KS_BUF_SIZE / 2; loop++) {
		buf[loop] = -1000.0 * amp;
	}
	for ( ; loop < KS_BUF_SIZE; loop++) {
		buf[loop] = 0.0;
	}
}

void addNewKSNote(uint32_t when_ms, float frequency, float amplitude) {
	if (frequency < 20 || frequency > 10000) return; // Sanity check

	// For simplicity, buffer numbers are currently derived
	// directly from the index number.  This should work as
	// long as MAX_KS_NOTES is a power of two.  Work out the
	// next buffer number, and turn off any note currently
	// using it, then assign the buffer to the new note:
	uint8_t bufferNumber = nextIndexNumber & 0x3;
	for (int loop = 0; loop < MAX_KS_NOTES; loop++) {
		if (KS_notes[loop].KSBufferNumber == bufferNumber && KS_notes[loop].status == Active)
			KS_notes[loop].status = Off;
	}
	KS_notes[nextIndexNumber].KSBufferNumber = nextIndexNumber % NUM_KS_BUFFERS;

	// Then add this new note in as pending, with the start time and amplitude:
	KS_notes[nextIndexNumber].status = Pending;
	KS_notes[nextIndexNumber].startTimeInMilliSeconds = when_ms;
	KS_notes[nextIndexNumber].samplesSinceStart = 0;
	KS_notes[nextIndexNumber].amplitude = amplitude;

	// Calculate the phase step:
	float phaseStep = (frequency * KS_BUF_SIZE) / (float)sampleRate;
	KS_notes[nextIndexNumber].mainPhaseStep = phaseStep;
	KS_notes[nextIndexNumber].mainPhaseOffset = 0;

	// And finally, update the next index number in a round-robin fashion:
	nextIndexNumber++; if (nextIndexNumber >= MAX_KS_NOTES) nextIndexNumber = 0;
}

void addKSsamples(uint16_t *buffer, uint32_t start, uint32_t end) {
	// This just looks through the possible notes and plays any that are
	// still going.  This should be a maximum of the number of buffers.

	for (int thisIndex = 0; thisIndex < MAX_KS_NOTES; thisIndex++) {

		// Work out which KS_Buffer this note is assigned to.  If it is not
		// assigned to an active buffer, turn the note off.
		float *KS_Buffer = KS_Buffers[0];
		uint16_t whichBuffer = KS_notes[thisIndex].KSBufferNumber % NUM_KS_BUFFERS;

		if (whichBuffer < 8) KS_Buffer = KS_Buffers[whichBuffer];
		else KS_notes[thisIndex].status = Off;

		// If this note is due to start, but is not currently playing,
		// then start it up:
		if (KS_notes[thisIndex].status == Pending) {
			if (HAL_GetTick() >= KS_notes[thisIndex].startTimeInMilliSeconds) {
				FillKSBuffer(KS_Buffer, KS_notes[thisIndex].amplitude);
				KS_notes[thisIndex].status = Active;
			}
		}

		// Then for any active notes, add their contribution to the audio buffer:
		if (KS_notes[thisIndex].status == Active) {
			for (int u = start; u < end; u += 2) {
				uint16_t currentOffset = (uint16_t)KS_notes[thisIndex].mainPhaseOffset;
				currentOffset = currentOffset & (KS_BUF_SIZE - 1);
				buffer[u] += (int16_t)KS_Buffer[currentOffset];
				buffer[u + 1] += (int16_t)KS_Buffer[currentOffset];
				KS_notes[thisIndex].mainPhaseOffset += KS_notes[thisIndex].mainPhaseStep;
				if (KS_notes[thisIndex].mainPhaseOffset >= KS_BUF_SIZE) {
					KS_notes[thisIndex].mainPhaseOffset -= KS_BUF_SIZE;

					// Finished one cycle through the buffer, so it's time to do the
					// filtering of the higher harmonics of the waveform:
					float lastElement = KS_Buffer[KS_BUF_SIZE - 1];
					for (int a = 1; a < KS_BUF_SIZE; a++) {
						KS_Buffer[a] = KS_Buffer[a] * one_minus_damping_factor + KS_Buffer[a - 1] * damping_factor;
					}
					KS_Buffer[0] = KS_Buffer[0] * one_minus_damping_factor + lastElement * damping_factor;
				}
			}
			KS_notes[thisIndex].samplesSinceStart += end - start;
		}
	}
}

