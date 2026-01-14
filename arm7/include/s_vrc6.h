#ifndef S_VRC6_H__
#define S_VRC6_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include "nestypes.h"
#include "audiosys.h"
#include "handler.h"
#include "nsf6502.h"
#include "nsdout.h"
#include "c_defs.h"

#define NES_BASECYCLES (21477270)
#define CPS_SHIFT 14

typedef struct 
{
    Uint32 cps;
    Int32 cycles;
    Uint32 spd;
    Uint8 regs[3];
    Uint8 adr;
    Uint8 mute;
} VRC6_SQUARE;

typedef struct 
{
    Uint32 cps;
    Int32 cycles;
    Uint32 spd;
    Uint32 output;
    Uint8 regs[3];
    Uint8 adr;
    Uint8 mute;
} VRC6_SAW;

typedef struct 
{
    VRC6_SQUARE square[2];
    VRC6_SAW saw;
    Uint32 mastervolume;
    Uint8 p_high; // Pulse Line High.
    Uint8 p_low;  // Pulse Line Low.
} VRC6SOUND;

void VRC6SoundSquareReset(VRC6_SQUARE *ch);
void VRC6SoundSawReset(VRC6_SAW *ch);
void VRC6SoundReset(void);

// VRC6 Init
void VRC6SoundInstall(void);
int32_t VRC6SoundRenderSquare1(void);
int32_t VRC6SoundRenderSquare2(void);
int32_t VRC6SoundRenderSaw(void);

// VRC6 Write regs
void VRC6SoundWrite9000(Uint address, Uint value);
void VRC6SoundWriteA000(Uint address, Uint value);
void VRC6SoundWriteB000(Uint address, Uint value);

#ifdef __cplusplus
}
#endif

#endif /* S_VRC6_H__ */
