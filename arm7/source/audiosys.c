#include "audiosys.h"
#include "s_defs.h"

/* ---------------------- */
/*  Audio Render Handler  */
/* ---------------------- */

Uint frequency = DS_SOUND_FREQUENCY;

// APU mixer status flags
enum apuRegion apuCurrentRegion = NTSC;
enum pulseCycles pulseCurrentStatus = Normal;
enum pulseMode CurrentPulseMode = PULSE_CH_SW;
bool stereo_enhanced = true;
// Sound Expansion flags
bool has_vrc6 = false;
bool has_fds  = false;

static NES_AUDIO_HANDLER *nah = 0;
// ARM7 side APU status flags
u32 apu_internal_state = 0;

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

enum pulseMode getPulseMode()
{
    return CurrentPulseMode;
}

enum apuRegion getApuCurrentRegion()
{
	return apuCurrentRegion;
}

/**
 * Calculates phase step in fixed point for the oscilators.
 * * Formula: (TotalCycles / (Divisor * OutputFrequency)) << Shift
 * * @param clock     Base clock Frequency (NES_BASECYCLES).
 * @param rate      Channel divisor * Output frequency (MIXFREQ).
 * @param shift     Fixed point accuracy (CPS_SHIFT).
 * @return          Phase step (cycles per sample) in fixed point.
 */
Uint32 getFixedPointStep(Uint32 clock, Uint32 rate, Uint32 shift)
{
    // We use 64 bits for the intermediate calculation to avoid overflows
    // before the division, allowing an accurate rounding.
    uint64_t clock_shifted = (uint64_t)clock << shift;
    
	// We add half of the divisor (rate / 2) to achieve rounding
	// to the nearest integer (nearest rounding) instead of truncation.
    return (Uint32)((clock_shifted + (rate >> 1)) / rate);
}

// Set APU status flags sent by the ARM9 to the ARM7 side
void applyApuStateMask(u32 mask) 
{
    apu_internal_state = mask;

    apuCurrentRegion   = (mask & APU_STAT_REGION_PAL) ? PAL : NTSC;
    pulseCurrentStatus = (mask & APU_STAT_DUTY_REV)   ? Reverse : Normal;
    CurrentPulseMode   = (mask & APU_STAT_PULSE_HW)   ? PULSE_CH_HW : PULSE_CH_SW;
    stereo_enhanced    = (mask & APU_STAT_STEREO)     ? true : false;
}
