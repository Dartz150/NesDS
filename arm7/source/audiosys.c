#include "audiosys.h"
#include "s_defs.h"

/* ---------------------- */
/*  Audio Render Handler  */
/* ---------------------- */

// APU mixer status flags
ApuConfig apu_cfg;

// Sound Expansion flags
bool has_vrc6;
bool has_fds;

static NES_AUDIO_HANDLER *nah = 0;
// ARM7 side APU status flags
u32 apu_internal_state;

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

void getPulseMode()
{
    return apu_cfg.hw_render;
}

void getApuCurrentRegion()
{
	return apu_cfg.region_pal;
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
    apu_internal_state   = mask;

	// Set flags into a struct from the FIFO msg received
    apu_cfg.region_pal   = (mask & APU_STAT_REGION_PAL)   ? true : false;
	apu_cfg.region_dendy = (mask & APU_STAT_REGION_DDY)   ? true : false; // TODO: ADD DENDY OVERRIDE
    apu_cfg.duty_reverse = (mask & APU_STAT_DUTY_REV)     ? true : false;
    apu_cfg.hw_render 	 = (mask & APU_STAT_PULSE_HW)     ? true : false;
    apu_cfg.stereo       = (mask & APU_STAT_STEREO)       ? true : false;
	apu_cfg.pu1raw 		 = (mask & APU_STAT_MUTE_P1) 	  ? true : false;
    apu_cfg.pu2raw 		 = (mask & APU_STAT_MUTE_P2) 	  ? true : false;
    apu_cfg.noiraw 		 = (mask & APU_STAT_MUTE_NOI)     ? true : false;
	apu_cfg.triraw       = (mask & APU_STAT_MUTE_TRI)  	  ? true : false;

	// Set Sound Ch. mute masks
    apu_cfg.pu1 		 = apu_cfg.pu1raw || apu_cfg.hw_render; // We also need to check if the hw render is enabled.
    apu_cfg.pu2 		 = apu_cfg.pu2raw || apu_cfg.hw_render;
    apu_cfg.noi		     = apu_cfg.noiraw || apu_cfg.hw_render;
	apu_cfg.tri 		 = apu_cfg.triraw || apu_cfg.hw_render;
    apu_cfg.dmc    	 	 = (mask & APU_STAT_MUTE_DMC)  	  ? true : false;
    apu_cfg.fds    	     = (mask & APU_STAT_MUTE_FDS) 	  ? true : false;
    apu_cfg.vrc_p1  	 = (mask & APU_STAT_MUTE_VRC_P1)  ? true : false;
    apu_cfg.vrc_p2  	 = (mask & APU_STAT_MUTE_VRC_P2)  ? true : false;
    apu_cfg.vrc_saw 	 = (mask & APU_STAT_MUTE_VRC_SAW) ? true : false;
}
