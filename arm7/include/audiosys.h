#ifndef AUDIOSYS_H__
#define AUDIOSYS_H__

#include <nds.h>
#include "nestypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (__fastcall *AUDIOHANDLER2)(Int32 *p);
typedef Int32 (__fastcall *AUDIOHANDLER)(void);

typedef struct NES_AUDIO_HANDLER_TAG {
	Uint fMode;
	AUDIOHANDLER Proc;
	AUDIOHANDLER2 Proc2;
	struct NES_AUDIO_HANDLER_TAG *next;
} NES_AUDIO_HANDLER;

typedef void (__fastcall *VOLUMEHANDLER)(Uint volume);

typedef struct NES_VOLUME_HANDLER_TAG {
	VOLUMEHANDLER Proc;
	struct NES_VOLUME_HANDLER_TAG *next;
} NES_VOLUME_HANDLER;
  
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
void NESAudioRender(s16 *bufp, Uint buflen);
void NESAudioHandlerInstall(NES_AUDIO_HANDLER *ph);
void NESAudioFrequencySet(Uint freq);
Uint NESAudioFrequencyGet(void);
void NESAudioChannelSet(Uint ch);
Uint NESAudioChannelGet(void);
void NESAudioHandlerInitialize(void);
void NESVolumeHandlerInstall(NES_VOLUME_HANDLER *ph);
void NESVolume(Uint volume);
void NESAudioFilterSet(Uint filter);
extern void (*FDSSoundWriteHandler)(Uint address, Uint value);
void FDSSoundInstall(void);
enum ApuRegion getApuCurrentRegion();
enum ApuStatus getApuCurrentStatus();
int32 Raw_PCM_Channel(unsigned char *buffer);

#ifdef __cplusplus
}
#endif

#endif /* AUDIOSYS_H__ */
