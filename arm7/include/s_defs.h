#pragma once
#include <nds/system.h>
#include "nestypes.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// NINTENDO DS AUDIO HARDWARE SOUND DEFINES
// =============================================================================
// Channels 0-1: Reserved for the Global Mixer in arm7main.c (PCM16 Stereo)
// Channels 2-7: PCM8/16 Only
// Channels 8-13: PCM8/16 & PSG Square Pulses
// Channels 14-15: PCM8/16 & PSG Noise (Unused, we use PCM8 for Noise)
// DMC NOTE: We need two channels for the DMC in order to have the max 127 volume range
// NOISE NOTE: We use PCM 8 for accurate NES Noise, sadly the DS noise channels (14-15) aren't suited for this.
// =============================================================================

// -----------------------------------------------------------------------------
// FIXED NES APU CHANNELS (Standard)
// -----------------------------------------------------------------------------
// PCM8 Channels (2-7)
#define DS_APU_TRIANGLE_CH     2            // PCM8 Triangle Wavetable Oscillator
#define DS_APU_DMC_CH_L        3            // PCM8 FastSlice DMC Buffer Left
#define DS_APU_DMC_CH_R        4            // PCM8 FastSlice DMC Buffer Right
#define DS_APU_NOISE_CH        5            // PCM8 FastSlice Noise Render 

// PSG-Capable Channels (8-13)
#define DS_APU_SQUARE_1_CH     DS_PSG_CH8   // PSG Hardware Square 1
#define DS_APU_SQUARE_2_CH     DS_PSG_CH9   // PSG Hardware Square 2

// -----------------------------------------------------------------------------
// SOUND EXPANSION CHANNELS (Shared)
// Note: Usually sound expansion chips never coexist, so they share these definitions.
// -----------------------------------------------------------------------------

// Expansion PCM8, can also be used as PCM16 if necessary (Slot 6, 7, 14 and 15 are free)
#define DS_EXP_PCM_SLOT_1      6
#define DS_EXP_PCM_SLOT_2      7
#define DS_EXP_PCM_SLOT_3      14
#define DS_EXP_PCM_SLOT_4      15

#define DS_VRC_SAW_CH          DS_EXP_PCM_SLOT_1  // PCM8 Saw Wavetable Oscillator
#define DS_MMC5_PCM_CH         DS_EXP_PCM_SLOT_1  // PCM Render

// Expansion PSG, can also be used as PCM8/16 if necessary (Slots 10-13 available for real-time Squares Waves)
#define DS_EXP_PSG_SLOT_1      DS_PSG_CH10
#define DS_EXP_PSG_SLOT_2      DS_PSG_CH11
#define DS_EXP_PSG_SLOT_3      DS_PSG_CH12
#define DS_EXP_PSG_SLOT_4      DS_PSG_CH13

#define DS_VRC_SQUARE_1_CH     DS_EXP_PSG_SLOT_1  // PCM8/PSG Hybrid Pulse Wavetable Oscillator 1
#define DS_VRC_SQUARE_2_CH     DS_EXP_PSG_SLOT_2  // PCM8/PSG Hybrid Pulse Wavetable Oscillator 2

#define DS_MMC5_SQUARE_1_CH    DS_EXP_PSG_SLOT_1  // PSG Hardware PSG Square
#define DS_MMC5_SQUARE_2_CH    DS_EXP_PSG_SLOT_2  // PSG Hardware PSG Square

#define DS_SS5B_SQ1_CH         DS_EXP_PSG_SLOT_1
#define DS_SS5B_SQ2_CH         DS_EXP_PSG_SLOT_2
#define DS_SS5B_SQ3_CH         DS_EXP_PSG_SLOT_3


// -----------------------------------------------------------------------------
// MIXER SETTINGS
// -----------------------------------------------------------------------------
#define DS_PAN_CENTER          64
#define DS_PAN_LEFT            0
#define DS_PAN_RIGHT           127

// Panning
#define DS_SQUARE_PAN_1_CH     DS_PAN_CENTER
#define DS_SQUARE_PAN_2_CH     DS_PAN_CENTER
#define DS_TRIANGLE_PAN_CH     DS_PAN_CENTER
#define DS_NOISE_PAN_CH        DS_PAN_CENTER
#define DS_DMC_PAN_L_CH        DS_PAN_LEFT
#define DS_DMC_PAN_R_CH        DS_PAN_RIGHT

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
// Total Samples the DS generates exactly in one frame
#define SAMPLES_PER_DS_FRAME           (((u64)DS_SOUND_FREQUENCY * NDS_CYCLES_PER_FRAME + (DS_BUS_CLOCK >> 1)) / DS_BUS_CLOCK)

#define MIXBUFSIZE                     (1 << 7)

#ifdef __cplusplus
}
#endif
