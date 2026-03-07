#include "audiosys.h"
#include "s_defs.h"

/* ---------------------- */
/*  Audio Render Handler  */
/* ---------------------- */

// APU mixer status flags
ApuConfig apu_cfg;

// Sound Expansion flags
bool has_mmc5;
bool has_vrc6;
bool has_fds;
bool has_ss5b;
bool has_n163;
bool has_vrc7;

// ARM7 side APU status flags
u32 apu_internal_state;

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

u16 nesToDsTimer(Uint32 nes_wl, Uint32 apu_clock, Uint8 clock_shift, Uint8 time_offset)
{
    uint64_t bus_clock = DS_BUS_CLOCK >> 1; // Sound hardware runs at half the DS Bus Clock
    uint32_t ratio = (bus_clock * clock_shift * (nes_wl + time_offset)) / apu_clock;

    return (u16)((DS_SOUND_FREQUENCY << 1) - ratio);
}

// Set APU status flags sent by the ARM9 to the ARM7 side
void applyApuStateMask(u32 mask) 
{
    apu_internal_state   = mask;

	// Set flags into a struct from the FIFO msg received
    apu_cfg.region_pal   = (mask & APU_STAT_REGION_PAL)    ? true : false;
	apu_cfg.region_dendy = (mask & APU_STAT_REGION_DDY)    ? true : false;
    apu_cfg.duty_reverse = (mask & APU_STAT_DUTY_REV)      ? true : false;
    apu_cfg.hw_render 	 = (mask & APU_STAT_PULSE_HW)      ? true : false;
    apu_cfg.stereo       = (mask & APU_STAT_STEREO)        ? true : false;

    // -- Standard NES APU --
	apu_cfg.pu1 		 = (mask & APU_STAT_MUTE_P1) 	   ? true : false;
    apu_cfg.pu2 		 = (mask & APU_STAT_MUTE_P2) 	   ? true : false;
	apu_cfg.tri          = (mask & APU_STAT_MUTE_TRI)  	   ? true : false;
    apu_cfg.dmc    	     = (mask & APU_STAT_MUTE_DMC)  	   ? true : false;
    apu_cfg.noi 		 = (mask & APU_STAT_MUTE_NOI)      ? true : false;

    // -- Sound Expansions --
    // FDS
    apu_cfg.fds          = (mask & APU_STAT_MUTE_FDS)      ? true : false;

    // VRC6
    apu_cfg.vrc_p1       = (mask & APU_STAT_MUTE_VRC_P1)   ? true : false;
    apu_cfg.vrc_p2       = (mask & APU_STAT_MUTE_VRC_P2)   ? true : false;
    apu_cfg.vrc_saw      = (mask & APU_STAT_MUTE_VRC_SAW)  ? true : false;

    // MMC5
    apu_cfg.mmc5_p1      = (mask & APU_STAT_MUTE_MMC5_P1)  ? true : false;
    apu_cfg.mmc5_p2      = (mask & APU_STAT_MUTE_MMC5_P2)  ? true : false;
    apu_cfg.mmc5_pcm     = (mask & APU_STAT_MUTE_MMC5_PCM) ? true : false;

    // SS5B
    apu_cfg.ss5b_p1      = (mask & APU_STAT_MUTE_SS5B_P1)  ? true : false;
    apu_cfg.ss5b_p2      = (mask & APU_STAT_MUTE_SS5B_P2)  ? true : false;
    apu_cfg.ss5b_p3      = (mask & APU_STAT_MUTE_SS5B_P3)  ? true : false;

    // N163
    apu_cfg.n163         = (mask & APU_STAT_MUTE_N163)     ? true : false;

    // VRC7
    apu_cfg.vrc7         = (mask & APU_STAT_MUTE_VRC7)     ? true : false;
}
