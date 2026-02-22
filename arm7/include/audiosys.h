#pragma once
#include <nds.h>
#include "nestypes.h"
#include "s_defs.h"
#include "SoundIPC.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NES_APU_NTSC 1789773
#define NES_APU_PAL  1662607

/* 31 - log2(NES_BASECYCLES/(12*MIN_FREQ)) > CPS_BITS  */
/* MIN_FREQ:11025 23.6 > CPS_BITS */
/* 32-12(max spd) > CPS_BITS */
#define CPS_SHIFT 16


typedef struct
{
    bool region_pal;     // 0 = NTSC, 1 = PAL
    bool region_dendy;   // 1 = Force NTSC audio over PAL
    bool duty_reverse;
    bool hw_render;
    bool stereo;

	// Sound Ch. mute flags
    bool pu1raw;
    bool pu2raw;
	bool triraw;
	bool pu1;
    bool pu2;
    bool noi;
	bool tri;
    bool dmc;
    bool fds;
    bool vrc_p1;
    bool vrc_p2;
    bool vrc_saw;
} ApuConfig;

void apuSoundWrite(Uint address, Uint value);
void apuVblankSync();
void APU4015Reg(void);
void applyApuStateMask(u32 mask);
Uint32 getFixedPointStep(Uint32 p1, Uint32 p2, Uint32 fix);
u16 nesToDsTimer(Uint32 nes_wl, Uint32 apu_clock, Uint8 clock_shift, Uint8 time_offset);

// APU mixer status flags
bool getApuCurrentRegion();
bool getPulseMode();

extern u32 apu_internal_state;
extern ApuConfig apu_cfg;
extern bool has_vrc6;
extern bool has_fds;

#ifdef __cplusplus
}
#endif
