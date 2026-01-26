#ifndef MIXER_H__
#define MIXER_H__

#include <nds/system.h>
#include "nestypes.h"

#ifdef __cplusplus
extern "C" {
#endif

// https://github.com/Gericom/GBARunner3/blob/develop/code/core/common/DsDefinitions.h

// DS Definitions

#define DS_BUS_CLOCK                   (33513982)

#define NDS_CYCLES_PER_PIXEL           6

#define NDS_LCD_WIDTH                  256
#define NDS_LCD_HBLANK                 99
#define NDS_LCD_COLUMNS                (NDS_LCD_WIDTH + NDS_LCD_HBLANK)

#define NDS_LCD_HEIGHT                 192
#define NDS_LCD_VBLANK                 71
#define NDS_LCD_LINES                  (NDS_LCD_HEIGHT + NDS_LCD_VBLANK)

#define NDS_CYCLES_PER_LINE            (NDS_LCD_COLUMNS * NDS_CYCLES_PER_PIXEL)
#define NDS_CYCLES_PER_FRAME           (NDS_LCD_COLUMNS * NDS_LCD_LINES * NDS_CYCLES_PER_PIXEL)

// DS output Frequency after mixing is 32.768 kHz 10 bits, this should be equal or below.
#define DS_SOUND_FREQUENCY             (32768)

// Proper rounding formula by "Asiekierka" of BlocksDS team https://github.com/blocksds/libnds/pull/49
#define TIMER_FREQ_SHIFT(n, divisor, shift) ((-((DS_BUS_CLOCK >> (shift)) * (divisor)) - ((((n) + 1)) >> 1)) / (n))

// From GBATEK: timerval = -(33513982Hz/2)/freq
#define TIMER_NFREQ                    (TIMER_FREQ_SHIFT(DS_SOUND_FREQUENCY, 1, 1))

// NES DEFINITIONS

#define NES_SCANLINES                  262

// MIXER DEFINITIONS

#define MIXBUFSIZE                     (1 << 8)
/*
 * DS Hardware Constants (from No$gba DS Sound docs: Channel/Mixer Bit-Widths section)
 * DS expects signed 16-bit PCM (SOUNDxCNT Format=1: PCM16, range -32768 to +32767).
 * We center post-mixer to bipolar for full dynamic range.
 */
#define GAIN       1      // Linear gain post-mixer (1 = full NES scale; >1 risks DS mixer overflow).
#define DC_OFFSET  16384  // Half of 32768: Centers unipolar NES output (0-32767) to bipolar (-16384 to +16383).
#define MAX_S16    32767  // 7FFFh: DS PCM16 max.
#define MIN_S16   -32768  // -8000h: DS PCM16 min.
// Total Samples the DS generates exactly in one frame
#define SAMPLES_PER_DS_FRAME           (DS_SOUND_FREQUENCY / (DS_BUS_CLOCK / NDS_CYCLES_PER_FRAME))

// struct unused yet
enum AudioFilterType
{
   NES_AUDIO_FILTER_NONE,
   NES_AUDIO_FILTER_CRISP,
   NES_AUDIO_FILTER_LOWPASS,
   NES_AUDIO_FILTER_HIGHPASS,
   NES_AUDIO_FILTER_WEIGHTED,
   NES_AUDIO_FILTER_OLDTV
};


// Pulse Channels 1 and 2 Software renderers
Int32 NESAPUSoundSquareRender1();
Int32 NESAPUSoundSquareRender2();

// Pulse Channels 1 and 2 Hardware renderers
// PSG channel writes change the sound INSTANTLY. 
// Always call this after the software sound renderers to avoid sound latency.
void NESAPUSoundSquareHWRender();
void NesAPUSoundSquareHWStop();

//Triangle/Noise/DMC Channels ("TND")
Int32 NESAPUSoundTriangleRender1();
Int32 NESAPUSoundNoiseRender1();
Int32 NESAPUSoundDpcmRender1();
void FDSSoundInstall();
void readAPU();

#ifdef __cplusplus
}
#endif

#endif /* MIXER_H__ */