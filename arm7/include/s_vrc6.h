#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// VRC6 Sound handlers
void VRC6SoundInstall(void);
void VRC6SoundReset(void);
int32_t VRC6SoundRender();

// VRC6 Write regs
void VRC6SoundWrite9000(Uint address, Uint value);
void VRC6SoundWriteA000(Uint address, Uint value);
void VRC6SoundWriteB000(Uint address, Uint value);

#ifdef __cplusplus
}
#endif
