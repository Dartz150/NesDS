#ifndef VRC7_ARM9_H
#define VRC7_ARM9_H

#include "soundChannel.h"
#include "nestypes.h"

#ifdef __cplusplus
extern "C" {
#endif

void vrc7SoundWrite(Uint address, Uint value);
void vrc7SoundInit();
int32_t vrc7SoundRender(void);

#ifdef __cplusplus
}
#endif

#endif