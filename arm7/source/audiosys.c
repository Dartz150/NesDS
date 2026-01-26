#include "audiosys.h"
#include "s_defs.h"

/* ---------------------- */
/*  Audio Render Handler  */
/* ---------------------- */

Uint frequency = DS_SOUND_FREQUENCY;

static NES_AUDIO_HANDLER *nah = 0;

static void NESAudioHandlerInstallOne(NES_AUDIO_HANDLER *ph)
{
	/* Add to tail of list*/
	ph->next = 0;
	if (nah)
	{
		NES_AUDIO_HANDLER *p = nah;
		while (p->next) p = p->next;
		p->next = ph;
	}
	else
	{
		nah = ph;
	}
}

void NESAudioHandlerInstall(NES_AUDIO_HANDLER *ph)
{
	for (;(ph->fMode&2)?(!!ph->Proc2):(!!ph->Proc);ph++) NESAudioHandlerInstallOne(ph);
}

void NESAudioFrequencySet(Uint freq)
{
	frequency = freq;
}

Uint NESAudioFrequencyGet(void)
{
	return frequency;
}

/**
 * Calculates phase step in fixed point for the oscilators.
 * * Formula: (TotalCycles / (Divisor * OutputFrequency)) << Shift
 * * @param clock     Base clock Frequency (NES_BASECYCLES).
 * @param rate      Channel divisor * Output frequency (MIXFREQ).
 * @param shift     Fixed point accuracy (CPS_SHIFT).
 * @return          Phase step (cycles per sample) in fixed point.
 */
Uint32 GetFixedPointStep(Uint32 clock, Uint32 rate, Uint32 shift)
{
    // We use 64 bits for the intermediate calculation to avoid overflows
    // before the division, allowing an accurate rounding.
    uint64_t clock_shifted = (uint64_t)clock << shift;
    
	// We add half of the divisor (rate / 2) to achieve rounding
	// to the nearest integer (nearest rounding) instead of truncation.
    return (Uint32)((clock_shifted + (rate >> 1)) / rate);
}