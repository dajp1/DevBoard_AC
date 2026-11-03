//  Source file for the FM synthesis routines.
//  Version 0.0.  DAJP - 24th Oct 2026.
#include "FM_synth.h"
#include "stm32g4xx_hal.h"

// The routines will need to know the sample rate:
static uint32_t sampleRate = 44100;
void setSampleRateFM(uint32_t newRate) { sampleRate = newRate; }

// Some other variables required, with some default values:
float sineBuffer[SINE_BUF_SIZE];
float mainEnvData[] = {1000.0, 1000.0, 0.8f, 88200.0, 441000.0};
float modEnvData[] = {1000.0, 1000.0, 0.8f, 22050.0, 176400.0};
float modulationFactor = 0.40;
float modulationFreqMultiplier = 2.0;
uint16_t nextFMToAdd = 0;
struct FM_noteType FM_notes[MAX_FM_NOTES];

// Fill the array of samples of the sine function, note all samples are
// between -1 and -1:
void fillSineArray() {
	double twopi = 6.28318530718;
	for (uint32_t loop = 0; loop < SINE_BUF_SIZE; loop++) {
		sineBuffer[loop] = (float)sin((float)loop * twopi / (float)SINE_BUF_SIZE);
	}
}

// Set the envelopes of the modulation and main operator.  These are stored locally
// in samples, which gives sub-millisecond accuracy for the envelopes.
void setEnvelope(bool bIsMain, float attackInMs, float decayInMs, float sustainLevel, float sustainInMs, float releaseInMs) {
	float endOfAttackInSamples = attackInMs * (float)sampleRate / 1000.0;
	float endOfDecayInSamples = (attackInMs + decayInMs) * (float)sampleRate / 1000.0;
	float endOfSustainInSamples = (attackInMs + decayInMs + sustainInMs) * (float)sampleRate / 1000.0;
	float endOfReleaseInSamples = (attackInMs + decayInMs + sustainInMs + releaseInMs) * (float)sampleRate / 1000.0;
	if (sustainLevel < 0.0) sustainLevel = 0.0;
	if (sustainLevel > 1.0) sustainLevel = 1.0;

	float *envArray = bIsMain ? mainEnvData : modEnvData;

	envArray[0] = endOfAttackInSamples;
	envArray[1] = endOfDecayInSamples;
	envArray[3] = endOfSustainInSamples;
	envArray[4] = endOfReleaseInSamples;
	envArray[2] = sustainLevel;
}

void setModulation(float modFactor, float freqMultiplier) {
	modulationFactor = modFactor;
	modulationFreqMultiplier = freqMultiplier;
}

// Add a new FM note to the current queue of notes:
void addNewFMNote(uint32_t when_ms, float frequency) {

	// Notes are allocated in round-robin fashion, so:
	uint16_t index = nextFMToAdd++;
	if (nextFMToAdd >= MAX_FM_NOTES) nextFMToAdd = 0;

	// Determine the steps for this frequency:
	float step = frequency * SINE_BUF_SIZE / sampleRate;
	float modStep = frequency * modulationFreqMultiplier * SINE_BUF_SIZE / sampleRate;

	// Can then add this note to the queue:
	FM_notes[index].status = Pending;
	FM_notes[index].mainPhaseStep = step;
	FM_notes[index].modPhaseStep = modStep;
	FM_notes[index].mainPhaseOffset = 0.0;
	FM_notes[index].modPhaseOffset = 0.0;
	FM_notes[index].lastMainEnv = 0.0;
	FM_notes[index].lastModEnv = 0.0;
	FM_notes[index].startTimeInMilliSeconds = when_ms;
	FM_notes[index].samplesSinceStart = 0;
}

// This is a helper function that calculates the envelope for a time (in ms)
// given an envelope array.  It returns a float between zero and one.
// The envelope array has terms [attack time; delay time; sustain level;
// sustain time; release time], with the sustain level between zero and one
// and the times in ms.
float getEnvelope(float *modEnvData, uint32_t samplesSinceStart) {
	float val = 0;

	if (samplesSinceStart < modEnvData[0]) {
		// In the attack phase:
		val = (float)samplesSinceStart / modEnvData[0];
	}
	else if (samplesSinceStart < modEnvData[1]) {
		// In the decay phase:
		float samplesIntoDecay = samplesSinceStart - modEnvData[0];
		float totalDecaySamples = modEnvData[1] - modEnvData[0];
		val = 1 - (1 - modEnvData[2]) * samplesIntoDecay / totalDecaySamples;
	}
	else if (samplesSinceStart < modEnvData[3]) {
		// In the sustain phase:
		val = modEnvData[2];
	}
	else if (samplesSinceStart < modEnvData[4]) {
		// In the release phase:
		float samplesIntoRelease = samplesSinceStart - modEnvData[3];
		float totalReleaseSamples = modEnvData[4] - modEnvData[3];
		val = modEnvData[2] * (1 - samplesIntoRelease / totalReleaseSamples);
	}

	return val < 0.0 ? 0.0 : val;
}

// Add the next lot of FM contributions to the audio buffer:
void addFMsamples(uint16_t *buffer, uint32_t start, uint32_t end) {
	// First, check to see which notes I should be playing.  For now,
	// this means finding the note with a start time closest to the
	// current time, and working backwards adding up the contributions
	// of up to four notes that are still going.

	uint32_t currentTime = HAL_GetTick();
	int16_t startHere = -1;
	uint32_t minTimePlaying = 0xFFFFFFFF;
	for (int q = 0; q < MAX_FM_NOTES; q++) {
		uint32_t startTimeMS = FM_notes[q].startTimeInMilliSeconds;
		if (currentTime > startTimeMS && currentTime - startTimeMS < minTimePlaying) {
			minTimePlaying = currentTime - startTimeMS;
			startHere = q;
		}
	}

	// If startHere is not -1, then might have found at least one active
	// note.  I'll try to play this one:
	if (startHere != -1) {
		int16_t thisIndex = startHere;

		for (int q = 0; q < 4; q++) {

			// If one of these notes is due to start, but not currently playing,
			// then start it up:
			if (FM_notes[thisIndex].status == Pending) {
				FM_notes[thisIndex].status = Active;
			}

			// Then for all active notes, add their contribution to the audio buffer:
			if (FM_notes[thisIndex].status == Active) {

				// To save time, the envelopes will only be calculated once every
				// buffer update time, and linearly interpolated otherwise.  This should
				// be enough for most cases, and much faster than recalculating the
				// envelopes every sample:
				FM_notes[thisIndex].samplesSinceStart += end - start;
				float newEnvMod = getEnvelope(modEnvData, FM_notes[thisIndex].samplesSinceStart);
				float newEnvMain = getEnvelope(mainEnvData, FM_notes[thisIndex].samplesSinceStart);

				// If the main envelope returns zero, then assume the release period
				// is over, and set the note to be off.
				if (newEnvMain == 0.0) FM_notes[thisIndex].status = Off;

				// Now work out the envelope factors for the first element in the buffer,
				// and the factor by which it changes per element:
				float startEnvMod = FM_notes[thisIndex].lastModEnv;
				float deltaEnvMod = (newEnvMod - startEnvMod) / (float)(end - start);
				FM_notes[thisIndex].lastModEnv = newEnvMod;

				float startEnvMain = FM_notes[thisIndex].lastMainEnv;
				float deltaEnvMain = (newEnvMain - startEnvMain) / (float)(end - start);
				FM_notes[thisIndex].lastMainEnv = newEnvMain;

				// Can now start to fill up the buffer for this note:
				for (int u = start; u < end; u += 2) {

					float modPhaseOffset = FM_notes[thisIndex].modPhaseOffset;
					float modSine = sineBuffer[((int16_t) modPhaseOffset) & SINE_BUF_SIZE_MINUS_ONE] * (startEnvMod + (u - start) * deltaEnvMod);
					float phaseMod = modulationFactor * modSine * SINE_BUF_SIZE;

					float mainPhaseOffset = FM_notes[thisIndex].mainPhaseOffset;
					mainPhaseOffset += phaseMod;

					// If is possible that this mainPhaseOffset is now outside the range of the sine
					// buffer, but converting to an integer and ANDing with the size of the buffer
					// minus one should get it back to a positive number in the right range provided
					// that SINE_BUF_SIZE is a power of two.
					float sample = sineBuffer[((int16_t) mainPhaseOffset) & SINE_BUF_SIZE_MINUS_ONE];

					// Then apply the envelope on the main oscillator, and scale:
					sample *= startEnvMain + (u - start) * deltaEnvMain;
					sample *= 1000; // Scale up from -1 to +1, with some head room for multiple notes

					buffer[u] += sample;
					buffer[u + 1] += sample;

					// Can't do the "& SINE_BUF_SIZE_MINUS_ONE" trick here, since these are floats,
					// but need to ensure that the phase offsets remain between zero and the SINE_BUF_SIZE.
					FM_notes[thisIndex].mainPhaseOffset += FM_notes[thisIndex].mainPhaseStep;
					if (FM_notes[thisIndex].mainPhaseOffset >= SINE_BUF_SIZE) {
						FM_notes[thisIndex].mainPhaseOffset -= SINE_BUF_SIZE;
					}
					FM_notes[thisIndex].modPhaseOffset += FM_notes[thisIndex].modPhaseStep;
					if (FM_notes[thisIndex].modPhaseOffset >= SINE_BUF_SIZE) {
						FM_notes[thisIndex].modPhaseOffset -= SINE_BUF_SIZE;
					}
				}
			}

			// Move back through the queue to the next index:
			thisIndex--; if (thisIndex < 0) thisIndex = MAX_FM_NOTES - 1;
		}
	}
}
