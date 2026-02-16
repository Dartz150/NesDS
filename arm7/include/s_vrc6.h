#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// VRC6 Sound handlers
void VRC6SoundInstall(void);
void VRC6SoundReset(void);
void VRC6SoundHwUpdate();
void VRC6SoundHwStop();

// VRC6 Write regs
void VRC6SoundWrite9000(Uint address, Uint value);
void VRC6SoundWriteA000(Uint address, Uint value);
void VRC6SoundWriteB000(Uint address, Uint value);

#ifdef __cplusplus
}
#endif
