#ifndef AUDIOSYS_H__
#define AUDIOSYS_H__

#include <nds.h>
#include "nestypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NES_BASECYCLES 21477272

/* 31 - log2(NES_BASECYCLES/(12*MIN_FREQ)) > CPS_BITS  */
/* MIN_FREQ:11025 23.6 > CPS_BITS */
/* 32-12(max spd) > CPS_BITS */
#define CPS_SHIFT 16

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

enum ApuStatus
{
	Reverse,
	Normal
};

void APU4015Reg(void);
void APUSoundInstall(void);
void NESAudioHandlerInstall(NES_AUDIO_HANDLER *ph);
void NESAudioFrequencySet(Uint freq);
Uint NESAudioFrequencyGet(void);
extern void (*FDSSoundWriteHandler)(Uint address, Uint value);
void FDSSoundInstall(void);
enum ApuRegion getApuCurrentRegion();
enum ApuStatus getApuCurrentStatus();
Uint32 GetFixedPointStep(Uint32 p1, Uint32 p2, Uint32 fix);

#ifdef __cplusplus
}
#endif

#endif /* AUDIOSYS_H__ */
