#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void fdsSoundInit(Uint32 nes_apu_clock, Uint32 ds_sound_freq);
void FDSSoundWrite(Uint address, Uint value);
Int32 FDSSoundRender(void);

#ifdef __cplusplus
}
#endif
