#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// VRC6 Sound handlers
void vrc6SoundInit();
void VRC6SoundHwUpdate(Uint32 nes_apu_clock, Uint32 ds_sound_freq);
void VRC6SoundHwStop();

// VRC6 Write regs
void VRC6SoundWrite9000(Uint address, Uint value);
void VRC6SoundWriteA000(Uint address, Uint value);
void VRC6SoundWriteB000(Uint address, Uint value);

#ifdef __cplusplus
}
#endif
