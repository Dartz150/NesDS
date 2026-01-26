#ifndef AUDIOSYS_H__
#define AUDIOSYS_H__

#include <nds.h>
#include "nestypes.h"
#include "s_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NES_CPU_NTSC 1789773
#define NES_CPU_PAL  1662607


/* 31 - log2(NES_BASECYCLES/(12*MIN_FREQ)) > CPS_BITS  */
/* MIN_FREQ:11025 23.6 > CPS_BITS */
/* 32-12(max spd) > CPS_BITS */
#define CPS_SHIFT 18

typedef void (__fastcall *AUDIOHANDLER2)(Int32 *p);
typedef Int32 (__fastcall *AUDIOHANDLER)(void);

typedef struct NES_AUDIO_HANDLER_TAG {
	Uint fMode;
	AUDIOHANDLER Proc;
	AUDIOHANDLER2 Proc2;
	struct NES_AUDIO_HANDLER_TAG *next;
} NES_AUDIO_HANDLER;

  
enum ApuRegion
{
	PAL,
	NTSC
};

enum ApuCycles
{
	Reverse,
	Normal
};

enum PulseMode
{
    PULSE_CH_SW,
    PULSE_CH_HW
};

void APU_VBlank_Sync();
void APU4015Reg(void);
void APUSoundInstall(void);
void NESAudioHandlerInstall(NES_AUDIO_HANDLER *ph);
void NESAudioFrequencySet(Uint freq);
Uint NESAudioFrequencyGet(void);
extern void (*FDSSoundWriteHandler)(Uint address, Uint value);
void FDSSoundInstall(void);
enum ApuRegion getApuCurrentRegion();
enum ApuCycles getApuCurrentStatus();
Uint32 GetFixedPointStep(Uint32 p1, Uint32 p2, Uint32 fix);

#ifdef __cplusplus
}
#endif

#endif /* AUDIOSYS_H__ */
