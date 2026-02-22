#pragma once
#include "blip_buf.h"

#ifdef __cplusplus
extern "C" {
#endif

void APU4015Reg(void);
void apuSoundWrite(Uint address, Uint value);
void apuSoundInit(Uint32 nes_apu_clock, Uint32 ds_sound_freq);

// Hardware renderers
// PSG channel writes change the sound INSTANTLY. 
// Always call this after the software sound renderers to avoid sound latency.
void nesApuSoundHwStop();

// Bip Buffer processes all the APU channels now
void nesApuProcessChannels(int sample_count, Uint32 nes_apu_clock, Uint32 ds_sound_freq);

extern blip_t* master_blip;
extern int ptr_mixed;

#ifdef __cplusplus
}
#endif
