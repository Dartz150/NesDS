#include <nds.h>
#include <string.h>
#include "nestypes.h"
#include "audiosys.h"
#include "handler.h"
#include "s_apu.h"
#include "c_defs.h"
#include "s_vrc6.h"
#include "s_fds.h"
#include "s_apu_defs.h"
#include "soundChannel.h"

// PSG Hardware Render Defines
#define PSG_APU_SQUARE_1_CH     DS_PSG_CH8
#define PSG_APU_SQUARE_2_CH     DS_PSG_CH9
#define PSG_APU_TRIANGLE_CH     DS_PSG_CH10
#define PSG_APU_NOISE_CH        DS_PSG_CH14
#define PSG_SQUARE_PAN_1_CH     64
#define PSG_SQUARE_PAN_2_CH     64
#define PSG_NOISE_PAN_CH        64
#define PSG_TRIANGLE_PAN_CH     64

// blip_buf Defines
#define DELTA_VOL 9
#define DMC_DELTA_VOL (DELTA_VOL - 1)

/* ------------------------- */
/*  NES INTERNAL SOUND(APU)  */
/* ------------------------- */

// Based from documentation found in https://www.nesdev.org/wiki/APU

/*/ Lenght Counter /*/
// Provides automatic duration control for the NES APU waveform channels ($4015 ~ $400F)

typedef struct 
{
	Uint32 counter;			/* length counter */
	Uint8 clock_disable;	/* length counter clock disable ($4015) */
} LENGTHCOUNTER;

// Linear Counter
typedef struct 
{
	Uint8 load;				/* length counter load register */
	Uint8 start;			/* length counter start */
	Uint8 counter;		    /* length counter */
	Uint8 tocount;		    /* length counter go to count mode */
	Uint8 mode;			    /* length counter mode load(0) count(1) */
	Uint8 clock_disable;	/* length counter clock disable */
} LINEARCOUNTER;

// Envelope Decay
typedef struct 
{
	Uint8 start;          /* envelope decay start flag */
	Uint8 disable;			/* envelope decay disable */
	Uint8 counter;			/* envelope decay counter */
	Uint8 rate;				/* envelope decay rate */
	Uint8 timer;			/* envelope decay timer */
	Uint8 looping_enable;	/* envelope decay looping enable */
	Uint8 volume;			/* volume */
} ENVELOPEDECAY;

// Sweep
typedef struct 
{
	Uint8 ch;				/* sweep channel */
	Uint8 active;			/* sweep active */
	Uint8 rate;				/* sweep rate */
	Uint8 timer;			/* sweep timer */
	Uint8 direction;		/* sweep direction */
	Uint8 shifter;			/* sweep shifter */
} SWEEP;

typedef struct 
{
	LENGTHCOUNTER lc;
	ENVELOPEDECAY ed;
	SWEEP sw;
	Uint32 mastervolume;
	Uint32 wl;				/* wave length */
	Uint32 pt;				/* programmable timer */
	Uint8 st;				/* wave step */
	Uint8 duty;				/* duty rate */
	Uint8 key;
	Uint8 mute;
	Uint32 last_amp;
} NESAPU_SQUARE;

typedef struct 
{
	LENGTHCOUNTER lc;		/* lenght counter */
	LINEARCOUNTER li;		/* linear counter */
	Uint32 mastervolume;	/* master volume (0x0 ~ +0x3FF) */
	Uint32 wl;				/* wave length */
	Uint32 pt;				/* programmable timer */
	Uint8 st;				/* wave step */
	Uint8 key;
	Uint8 mute;
	Uint32 last_amp;
} NESAPU_TRIANGLE;

typedef struct 
{
	LENGTHCOUNTER lc;
	LINEARCOUNTER li;
	ENVELOPEDECAY ed;
	Uint32 mastervolume;
	Uint32 wl;				/* wave length */
	Uint32 pt;				/* programmable timer */
	Uint32 rng;
	Uint8 rngshort;
	Uint8 key;
	Uint8 mute;
	Uint32 last_amp;
} NESAPU_NOISE;

typedef struct 
{
	Uint32 wl;				/* wave length */
	Uint32 pt;				/* programmable timer */
	Uint32 length;			/* bit length */
	Uint32 mastervolume;
	Uint32 adr;				/* current address */
	Int32 dacout;
	Int32 dacout0;
	Uint8 start_length;
	Uint8 start_adr;
	Uint8 loop_enable;
	Uint8 irq_enable;
	Uint8 irq_report;
	Uint8 input;			/* 8bit input buffer */
	Uint8 first;
	Uint8 bit_count;          // 8bit internal counter TODO:Render RAW PCM properly
	Uint8 dacbase;
	Uint8 key;
	Uint8 mute;
	Uint32 last_amp;
} NESAPU_DPCM;

typedef struct 
{
	NESAPU_SQUARE square[2];
	NESAPU_TRIANGLE triangle;
	NESAPU_NOISE noise;
	NESAPU_DPCM dpcm;
    Uint32 fc;    			/* Global Frame Counter */
    Uint32 fp;    			/* Global Frame Position */
	Uint8 regs[0x20];
} APUSOUND __attribute__((aligned(32)));

static APUSOUND apu;
static int  apuirq;
static blip_t* master_blip;
static int ptr_mixed;

// Square Duty LUT
static const Uint8 square_duty_table_normal[4] = 
{ 
	0x02, 0x04, 0x08, 0x0C
};

static const Uint8 square_duty_table_inverted[4] = 
{ 
	0x0C, 0x08, 0x04, 0x02
};

// APU_Length_Counter LUT ($400F)
[[gnu::aligned(4)]]
static const Uint8 vbl_length_table[32] = 
{
	0x0A, 0xFE, 0x14, 0x02, 0x28, 0x04, 0x50, 0x06,
	0xA0, 0x08, 0x3C, 0x0A, 0x0E, 0x0C, 0x1A, 0x0E,
	0x0C, 0x10, 0x18, 0x12, 0x30, 0x14, 0x60, 0x16,
	0xC0, 0x18, 0x48, 0x1A, 0x10, 0x1C, 0x20, 0x1E
};

// APU Noise Time Period LUT NTSC ($400E)
[[gnu::aligned(2)]]
static const Uint32 noise_time_period_table_ntsc[16] =
{
	0x004, 0x008, 0x010, 0x020, 0x040, 0x060, 0x080, 0x0A0,
	0x0CA, 0x0FE, 0x17C, 0x1FC, 0x2FA, 0x3F8, 0x7F2, 0xFE4
};

// APU Noise Time Period LUT PAL ($400E)
[[gnu::aligned(2)]]
static const Uint32 noise_time_period_table_pal[16] =
{
    0x004, 0x008, 0x00E, 0x01E, 0x03C, 0x058, 0x076, 0x094,
    0x0BC, 0x0EC, 0x162, 0x1D8, 0x2C4, 0x3B0, 0x762, 0xEC2
};

// APU DMC LUT NTSC ($4010)
[[gnu::aligned(2)]]
static const Uint32 dpcm_freq_table_ntsc[16] =
{
	0x1AC, 0x17C, 0x154, 0x140, 0x11E, 0x0FE, 0x0E2, 0x0D6,
	0x0BE, 0x0A0, 0x08E, 0x080, 0x06A, 0x054, 0x048, 0x036
};

// APU DMC LUT PAL
[[gnu::aligned(2)]]
static const Uint32 dpcm_freq_table_pal[16] =
{
	0x18E, 0x162, 0x13C, 0x12A, 0x114, 0x0EC, 0x0D2, 0x0C6,
	0x0B0, 0x094, 0x084, 0x076, 0x062, 0x04E, 0x042, 0x032
};

// NTSC: 1 APU cycle ≈ 2 CPU cycles (1.78 MHz)
static const Uint32 frame_seq_ntsc_4[4] = // NTSC 4 step mode 
{
    7457, 14913, 22371, 29829
};

static const Uint32 frame_seq_ntsc_5[5] = // NTSC 5 step mode 
{ 
    7457, 14913, 22371, 29829, 37281
}; 

// PAL: 1 APU cycle ≈ 2 CPU cycles (1.66 MHz)
static const Uint32 frame_seq_pal_4[4]  = // PAL 4 step mode 
{
    8313, 16627, 24939, 33253
};

static const Uint32 frame_seq_pal_5[5]  = // PAL 5 step mode 
{
    8313, 16627, 24939, 33253, 41565
};

[[gnu::aligned(4)]]
static const s8 sNesTriangleTable[] =
{
    -64, -56, -48, -40, -32, -24, -16,  -8,   0,   8,  16,  24,  32,  40,  48,  56,
     64,  56,  48,  40,  32,  24,  16,   8,   0,  -8, -16, -24, -32, -40, -48, -56,
    -64, -56, -48, -40, -32, -24, -16,  -8,   0,   8,  16,  24,  32,  40,  48,  56,
     64,  56,  48,  40,  32,  24,  16,   8,   0,  -8, -16, -24, -32, -40, -48, -56
};

[[gnu::aligned(4)]]
static const s8 sNesSawTable[] =
{
    -64, -62, -60, -58, -56, -54, -52, -50,
    -48, -46, -44, -42, -40, -38, -36, -34,
    -32, -30, -28, -26, -24, -22, -20, -18,
    -16, -14, -12, -10,  -8,  -6,  -4,  -2,
      0,   2,   4,   6,   8,  10,  12,  14,
     16,  18,  20,  22,  24,  26,  28,  30,
     32,  34,  36,  38,  40,  42,  44,  46,
     48,  50,  52,  54,  56,  58,  60,  62
};

[[gnu::aligned(4)]]
static const u8 sNesNoiseShortTable[] =
{
    // Feedback = bit0 ^ bit6 (93 step cycle * 4 + interpolation filter)
    0x20,0x60,0x7F,0x7F, 0x7F,0x7F,0x7F,0x7F, 0x7F,0x7F,0x7F,0x7F, 0x7F,0x7F,0x7F,0x7F,
    0x7F,0x7F,0x7F,0x7F, 0x7F,0x7F,0x7F,0x7F, 0x60,0x20,0x00,0x00, 0x00,0x00,0x00,0x00,
    0x20,0x60,0x7F,0x7F, 0x7F,0x7F,0x7F,0x7F, 0x7F,0x7F,0x7F,0x7F, 0x60,0x20,0x00,0x00,
    0x00,0x00,0x00,0x00, 0x20,0x60,0x7F,0x7F, 0x7F,0x7F,0x7F,0x7F, 0x7F,0x7F,0x7F,0x7F,
    0x60,0x20,0x00,0x00, 0x20,0x60,0x7F,0x7F, 0x60,0x20,0x00,0x00, 0x20,0x60,0x7F,0x7F,
    0x7F,0x7F,0x7F,0x7F, 0x60,0x20,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00, 0x20,0x60,0x7F,0x7F, 0x7F,0x7F,0x7F,0x7F, 0x60,0x20,0x00,0x00,
    0x20,0x60,0x7F,0x7F, 0x7F,0x7F,0x7F,0x7F, 0x7F,0x7F,0x7F,0x7F, 0x60,0x20,0x00,0x00,
    0x20,0x60,0x7F,0x7F, 0x60,0x20,0x00,0x00, 0x20,0x60,0x7F,0x7F, 0x7F,0x7F,0x7F,0x7F,
    0x60,0x20,0x00,0x00, 0x00,0x00,0x00,0x00, 0x20,0x60,0x7F,0x7F, 0x7F,0x7F,0x7F,0x7F,
    0x7F,0x7F,0x7F,0x7F, 0x60,0x20,0x00,0x00, 0x20,0x60,0x7F,0x7F, 0x60,0x20,0x00,0x00,
    0x20,0x60,0x7F,0x7F, 0x60,0x20,0x00,0x00, 0x00,0x00,0x00,0x00, 0x20,0x60,0x7F,0x7F,
    0x60,0x20,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
    0x20,0x60,0x7F,0x7F, 0x7F,0x7F,0x7F,0x7F, 0x60,0x20,0x00,0x00, 0x20,0x60,0x7F,0x7F,
    0x7F,0x7F,0x7F,0x7F, 0x7F,0x7F,0x7F,0x7F, 0x60,0x20,0x00,0x00, 0x20,0x60,0x7F,0x7F,
    0x60,0x20,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x20,0x60,0x7F,0x7F,
    0x7F,0x7F,0x7F,0x7F, 0x60,0x20,0x00,0x00, 0x00,0x00,0x00,0x00, 0x20,0x60,0x7F,0x7F,
    0x60,0x20,0x00,0x00, 0x00,0x00,0x00,0x00, 0x20,0x60,0x7F,0x7F, 0x60,0x20,0x00,0x00,
    0x20,0x60,0x7F,0x7F, 0x60,0x20,0x00,0x00, 0x20,0x60,0x7F,0x7F, 0x7F,0x7F,0x7F,0x7F,
    0x60,0x20,0x00,0x00, 0x20,0x60,0x7F,0x7F, 0x60,0x20,0x00,0x00, 0x00,0x00,0x00,0x00,
    0x20,0x60,0x7F,0x7F, 0x7F,0x7F,0x7F,0x7F, 0x60,0x20,0x00,0x00, 0x20,0x60,0x7F,0x7F,
    0x7F,0x7F,0x7F,0x7F, 0x60,0x20,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
    0x20,0x60,0x7F,0x7F, 0x60,0x20,0x00,0x00, 0x20,0x60,0x7F,0x7F, 0x20,0x60,0x7F,0x7F
};

static const Uint8  *square_duty_table;
static const Uint32 *noise_time_period_table;
static const Uint32 *dpcm_freq_table;
static const Uint32 *frame_seq_4;
static const Uint32 *frame_seq_5;

__inline static void lengthCounterStep(LENGTHCOUNTER *lc)
{
	if (lc->counter && !lc->clock_disable) 
	{
		lc->counter--;
	}
}

// We no longer need cps calculations now, since blip doesn't render per-sample
__inline static void linearCounterStepBlip(LINEARCOUNTER *li)
{
    // Reload if flag is enabled
    if (li->tocount)
    {
        li->counter = li->load;
    }
    // Decrement if not zero or if it's more than zero
    else if (li->counter > 0)
    {
        li->counter--;
    }
    // Spec: "If the control flag (clock_disable) is clear, 
    // the linear counter reload flag is cleared."
    if (!li->clock_disable)
    {
        li->tocount = 0;
    }
}

__inline static void envelopeDecayStep(ENVELOPEDECAY *ed)
{
    if (ed->start) {
        ed->start = 0;
        ed->counter = 0xF;
        ed->timer = ed->rate; // Initialize with rate so the first step is immediate
    } else {
        if (ed->timer > 0) {
            ed->timer--;
        } else {
            ed->timer = ed->rate; // Reload with V (Rate)
            if (ed->counter > 0) {
                ed->counter--;
            } else if (ed->looping_enable) {
                ed->counter = 0xF;
            }
        }
    }
}

__inline static void sweepStep(SWEEP *sw, Uint32 *wl)
{
	if (sw->active && sw->shifter && ++sw->timer > sw->rate)
	{
		sw->timer = 0;
		if (sw->direction)
		{
			*wl -= (*wl >> sw->shifter);
			if (*wl && !sw->ch) 
			{
				(*wl)--;
			}
		}
		else
		{
			*wl += (*wl >> sw->shifter);
		}
	}
}

__inline static u16 nesToDsTimer(Uint32 nes_wl, Uint32 apu_clock, Uint8 clock_shift, Uint8 time_offset)
{
    uint64_t bus_clock = DS_BUS_CLOCK >> 1; // Sound hardware runs at half the DS Bus Clock
    uint32_t ratio = (bus_clock * clock_shift * (nes_wl + time_offset)) / apu_clock;

    return (u16)((DS_SOUND_FREQUENCY << 1) - ratio);
}

__inline static u32 nesDutyToDs(u8 duty_value)
{
    // duty_values derived from square_duty_table[4]
    switch(duty_value)
	{
        case 0x02: // 2/16 steps = 12.5%
            return SOUNDCNT_DUTY_12_5;
        case 0x04: // 4/16 steps = 25%
            return SOUNDCNT_DUTY_25_0;
        case 0x08: // 8/16 steps = 50%
            return SOUNDCNT_DUTY_50_0;
        case 0x0C: // 12/16 steps = 75%
            return SOUNDCNT_DUTY_75_0;
        default:
            return SOUNDCNT_DUTY_12_5;
    }
}

/// @brief Generates and updates a pulse wave using the Nintendo DS PSG hardware channel.
/// @param ch      Pointer to the NES square channel state structure.
/// @param ds_chan Nintendo DS hardware channel index used for PSG output.
///                Valid range: 9 to 15 (only these channels support PSG mode).
/// @param pan     Stereo panning value (0 = full left, 64 = center, 127 = full right).
static void nesApuSoundPulseUpdateHw(NESAPU_SQUARE *ch, DS_PSG_Channel ds_chan, int pan, u32 apu_clock)
{
    // Sweep, lenght and decay are now updated globally in nesApuProcessBlipBufferChannels()
    // We just need to read the updated values. 

    // Calculate target wavelength to handle Sweep
    u32 delta_wl = ch->wl >> ch->sw.shifter;
    u32 sweep_target = ch->wl;
    if (ch->sw.direction)
    {
        sweep_target -= delta_wl;
        if (ch == &apu.square[0] && sweep_target > 0)
        {
            sweep_target--; 
        }
    }
    else
    {
        sweep_target += delta_wl;
    }
    // Immediate silence if wavelength is out of range, length counter is zero, or key is off
    bool is_silenced = (ch->wl < 8 || sweep_target > 0x7FF || ch->lc.counter == 0 || !ch->key || ch->mute);

    if (is_silenced) 
    {
        if (snd_isChannelPlaying(ds_chan)) snd_stopChannel(ds_chan);
        return;
    }

    // Convert wavelenght, duty and volume parameters to the DS PSG hardware channel
    u16 ds_timer = nesToDsTimer(ch->wl, apu_clock, 2, 1);
    u32 ds_duty = nesDutyToDs(ch->duty);
    u8  volume  = ch->ed.disable ? ch->ed.volume : ch->ed.counter;
    u8  ds_vol  = volume << 1;

    REG_SOUNDxTMR(ds_chan) = ds_timer;

    if (!snd_isChannelPlaying(ds_chan))
    {
        REG_SOUNDxTMR(ds_chan) = ds_timer;
        REG_SOUNDxCNT(ds_chan) = SOUNDCNT_ENABLED | SOUNDCNT_FORMAT_PSG | 
            SOUNDCNT_MODE_LOOP | ds_duty | 
            SOUNDCNT_PAN(pan) | SOUNDCNT_VOLUME(ds_vol);
    }
    else
    {
        u32 current_cnt = REG_SOUNDxCNT(ds_chan);
        u32 new_cnt = (current_cnt & ~(0x7F | SOUNDCNT_DUTY_MASK)) | ds_vol | ds_duty;
        if (current_cnt != new_cnt)
        {
            REG_SOUNDxCNT(ds_chan) = new_cnt;
        }
    }
}

/// @brief Updates a triangle wave using a triangle wave pre-computed in a s8 table.
/// @param ch      Pointer to the NES triangle channel state structure.
/// @param ds_chan Nintendo DS hardware channel index used for PCM8 output.
/// @param pan     Stereo panning value (0 = full left, 64 = center, 127 = full right).
static void nesApuSoundTriangleUpdateHw(NESAPU_TRIANGLE *ch, DS_PSG_Channel ds_chan, int pan, u32 apu_clock)
{
    // NES Triangle has two counters: 
    // 1. Length Counter (lc)
    // 2. Linear Counter (li)
    // If any is == 0, we must silence it.
    bool is_silenced = (ch->wl < 2 || ch->lc.counter == 0 || ch->li.counter == 0 || ch->mute);

    if (is_silenced) 
    {
        if (snd_isChannelPlaying(ds_chan)) snd_stopChannel(ds_chan);
        return;
    }

    // Clock_shift = 1 (Square * 2)
    u16 ds_timer = nesToDsTimer(ch->wl, apu_clock, 1, 1);
    
    // We need to cut the volume at ~75% because the table is interpoplated for normalization
    u32 ds_vol = SOUNDCNT_VOLUME(80);

    if (!snd_isChannelPlaying(ds_chan))
    {
        REG_SOUNDxSAD(ds_chan) = (u32)sNesTriangleTable;
        REG_SOUNDxPNT(ds_chan) = 0;
        REG_SOUNDxLEN(ds_chan) = sizeof(sNesTriangleTable) >> 2;
        REG_SOUNDxTMR(ds_chan) = ds_timer;
        REG_SOUNDxCNT(ds_chan) = SOUNDCNT_ENABLED | SOUNDCNT_FORMAT_PCM8 | 
                                 SOUNDCNT_MODE_LOOP | SOUNDCNT_PAN(pan) | ds_vol;
    }
    else
    {
        // Update freq and volume
        REG_SOUNDxTMR(ds_chan) = ds_timer;
        
        u32 current_cnt = REG_SOUNDxCNT(ds_chan);
        u32 new_cnt = (current_cnt & ~0x7F) | ds_vol;
        if (current_cnt != new_cnt)
        {
            REG_SOUNDxCNT(ds_chan) = new_cnt;
        }
    }
}

/// @brief Updates the noise using a PSG channel and a pre-computed interpolated u8 noise table 
///        for the 7-bit short mode, and the PSG hardware for the 15-bit mode.
/// @param ch      Pointer to the NES noise channel state structure.
/// @param ds_chan Nintendo DS hardware channel index used for PCM8/PSG output.
/// @param pan     Stereo panning value (0 = full left, 64 = center, 127 = full right).
static void nesApuSoundNoiseUpdateHw(NESAPU_NOISE *ch, DS_PSG_Channel ds_chan, int pan, u32 apu_clock)
{
    u8 volume = ch->ed.disable ? ch->ed.volume : ch->ed.counter;
    bool is_silenced = (ch->lc.counter == 0 || volume == 0 || ch->mute);

    if (is_silenced) 
    {
        if (snd_isChannelPlaying(ds_chan)) snd_stopChannel(ds_chan);
        return;
    }

    u32 ds_vol = SOUNDCNT_VOLUME(volume << 2);
    u32 ds_pan = SOUNDCNT_PAN(pan);
    u32 current_cnt = REG_SOUNDxCNT(ds_chan);

    u16 ds_timer_long = nesToDsTimer(ch->wl, apu_clock, 1, 0);
    u16 ds_timer_short = nesToDsTimer((ch->wl >> 2), apu_clock, 1, 0);

    if (ch->rngshort) 
    {
        // --- SHORT MODE ("Metallic") 4x Oversampled ---
        
        //    Calculate Timer adjusted for data x4.
        //    The table has 4 samples for every NES "tick", 
        //    hence we need to read 4x the speed to keep the tone.
        bool is_pcm = (current_cnt & SOUNDCNT_FORMAT_PSG) == SOUNDCNT_FORMAT_PCM8;

        if (!snd_isChannelPlaying(ds_chan) || !is_pcm)
        {
            snd_stopChannel(ds_chan);
            REG_SOUNDxSAD(ds_chan) = (u32)sNesNoiseShortTable;
            REG_SOUNDxPNT(ds_chan) = 0;
            
            // Length is still 93 words (372 bytes), hardware will
            // wrap-around automatically to the start of the table
            // If we change SAD, we also need to change the loop start.
            REG_SOUNDxLEN(ds_chan) = sizeof(sNesNoiseShortTable) >> 2; // 372 bytes / 4
            REG_SOUNDxTMR(ds_chan) = ds_timer_short;
            REG_SOUNDxCNT(ds_chan) = SOUNDCNT_ENABLED | SOUNDCNT_FORMAT_PCM8 | 
                                     SOUNDCNT_MODE_LOOP | ds_pan | ds_vol;
        }
        else
        {
            // Update timer and volume
            REG_SOUNDxTMR(ds_chan) = ds_timer_short;
            REG_SOUNDxCNT(ds_chan) = (current_cnt & ~0x7F) | ds_vol;
        }
    }
    else 
    {
        // --- LONG MODE (White noise) ---
        bool is_psg = (current_cnt & SOUNDCNT_FORMAT_PSG) == SOUNDCNT_FORMAT_PSG;

        if (!snd_isChannelPlaying(ds_chan) || !is_psg)
        {
            snd_stopChannel(ds_chan);
            REG_SOUNDxTMR(ds_chan) = ds_timer_long;
            REG_SOUNDxCNT(ds_chan) = SOUNDCNT_ENABLED | SOUNDCNT_FORMAT_PSG | 
                                     SOUNDCNT_MODE_LOOP | ds_pan | ds_vol >> 1;
        }
        else
        {
            REG_SOUNDxTMR(ds_chan) = ds_timer_long;
            REG_SOUNDxCNT(ds_chan) = (current_cnt & ~0x7F) | ds_vol >> 1;
        }
    }
}

/// @brief Update the DS hardware channel renders. Writes change the sound INSTANTLY.
//         Always call this after the software sound renderers to avoid sound latency.
/// @param nes_apu_clock NES APU clock frequency.
__inline static void nesApuSoundHwRender(uint32_t nes_apu_clock)
{
    u32 pu1_pan, pu2_pan;
    if (apu_cfg.stereo)
    {
        pu1_pan = 96;
        pu2_pan = 32;
    }
    else
    {
        pu1_pan = 64;
        pu2_pan = 64;
    }
	// Check if the APU flags have any of the channels muted
    (apu_cfg.pu1raw)
        ? snd_stopChannel(PSG_APU_SQUARE_1_CH)
        : nesApuSoundPulseUpdateHw(&apu.square[0], PSG_APU_SQUARE_1_CH, pu1_pan, nes_apu_clock);

    (apu_cfg.pu2raw)
        ? snd_stopChannel(PSG_APU_SQUARE_2_CH)
        : nesApuSoundPulseUpdateHw(&apu.square[1], PSG_APU_SQUARE_2_CH, pu2_pan, nes_apu_clock);

    (apu_cfg.triraw)
        ? snd_stopChannel(PSG_APU_TRIANGLE_CH)
        : nesApuSoundTriangleUpdateHw(&apu.triangle, PSG_APU_TRIANGLE_CH, PSG_TRIANGLE_PAN_CH, nes_apu_clock);   

    (apu_cfg.noiraw)
        ? snd_stopChannel(PSG_APU_NOISE_CH)
        : nesApuSoundNoiseUpdateHw(&apu.noise, PSG_APU_NOISE_CH, PSG_NOISE_PAN_CH, nes_apu_clock);
}

/// @brief Stops all the hardware DS channels
void nesApuSoundHwStop()
{
    snd_stopChannel(PSG_APU_SQUARE_1_CH);
    snd_stopChannel(PSG_APU_SQUARE_2_CH);
    snd_stopChannel(PSG_APU_TRIANGLE_CH);
    snd_stopChannel(PSG_APU_NOISE_CH);
}

__inline static void nesApuBlipInit(int apu_clock_rate, int sample_rate)
{
    // Reset ch counters
    apu.square[0].last_amp = 0;
    apu.square[1].last_amp = 0;
	apu.triangle.last_amp = 0;
	apu.noise.last_amp = 0;
	apu.dpcm.last_amp = 0;

    // Init blip_buff parameters
    if (master_blip)
    {
        blip_delete(master_blip);
    }
    master_blip = blip_new(MIXBUFSIZE);
	blip_set_rates(master_blip, apu_clock_rate, sample_rate);
}

__inline static void nesApuSoundPulseRenderBlipSlice(NESAPU_SQUARE *ch, blip_t* blip_buffer, int clocks, int time_offset, bool is_muted)
{   
    if (is_muted || clocks <= 0)
    { 
        return;
    }

    // Verify NES hardware limits
    u32 delta_wl = ch->wl >> ch->sw.shifter;
    u32 sweep_target = ch->wl;
    if (ch->sw.direction)
    {
        sweep_target -= delta_wl;
        if (ch == &apu.square[0] && sweep_target > 0)
        {
            sweep_target--;
        }
    }
    else
    {
        sweep_target += delta_wl;
    }
    
    // Sweep silences the channel if the WL is > 0x7FF or < 8
    bool hardware_silence = (ch->wl < 8 || (!ch->sw.direction && sweep_target > 0x7FF));
	bool silent = (!ch->key || !ch->lc.counter || ch->mute || hardware_silence);

    if (silent)
    {
        if (ch->last_amp != 0)
        {
            blip_add_delta(blip_buffer, time_offset, -(ch->last_amp) << DELTA_VOL);
            ch->last_amp = 0;
        }
        // IMPORTANT: Sum clocks to the pt remainder instead of resetting to 0.
        // This avoids shorter lenght notes in certain scenarios.
        ch->pt += clocks;
        
        // https://www.nesdev.org/wiki/APU#Pulse_($4000%E2%80%93$4007)
	    // f = fCPU / (16 × (t + 1))
        u32 period = (ch->wl + 1);
        if (ch->pt > (period << 4)) ch->pt %= (period << 4); 
        return;
    }
    // https://www.nesdev.org/wiki/APU#Pulse_($4000%E2%80%93$4007)
	// f = fCPU / (16 × (t + 1))
    u32 period = (ch->wl + 1);
    if (period < 8) // Safety clamp
    {
        period = 8;
    }
    
    int current_vol = ch->ed.disable ? ch->ed.volume : ch->ed.counter;
    u32 time_at_delta = 0;
    
    // Generate amplitude
    // We use the remainder in ch->pt
    while (time_at_delta < (u32)clocks)
    {
        // Generate Wave: Duty cycle sequencer (16 steps).
        int amp = (ch->st >= ch->duty) ? current_vol : 0;
        if (amp != ch->last_amp)
        {
            blip_add_delta(blip_buffer, time_at_delta + time_offset, (amp - ch->last_amp) << DELTA_VOL);
            ch->last_amp = amp;
        }

        u32 time_to_next_step = (period > ch->pt) ? (period - ch->pt) : 0;

        if (time_at_delta + time_to_next_step > (u32)clocks)
        {
            ch->pt += ((u32)clocks - time_at_delta);
            break;
        }

        time_at_delta += time_to_next_step;
        ch->st = (ch->st + 1) & 0xF; // 16 steps cycle
        ch->pt = 0; // Reset only when a cycle finishes
    }
}

__inline static void nesApuSoundTriangleRenderBlipSlice(NESAPU_TRIANGLE *ch, blip_t* blip_buffer, int clocks, int time_offset, bool is_muted)
{
    if (is_muted || clocks <= 0)
    {
        return;
    }

    // Spec: The sequencer only advances if BOTH are more than zero.
	//      Linear Counter   Length Counter
	//             |                |
	//             v                v
	// Timer ---> Gate ----------> Gate ---> Sequencer ---> (to mixer)
    // Triangle actually freezes, it doesn't get silenced.
    bool frozen = (ch->lc.counter == 0 || ch->li.counter == 0 || ch->wl < 2);
    if (frozen || ch->mute)
    {
        // We don't sum clocks to ch->pt. Oscilator remains still.
        return; 
    }

    u32 period = (ch->wl + 1);
    u32 time_at_delta = 0;
    if (ch->pt >= period)
    {
        ch->pt %= period;
    }

    while (time_at_delta < (u32)clocks)
    {
        // Generate 32 step wave (0-15-0)
        int step_val = ch->st;
        int amp = (step_val & 0x10) ? (0x1F - step_val) : step_val;  // 32 step cycle (0-31), invert to create slope

        if (amp != ch->last_amp)
        {
            blip_add_delta_fast(blip_buffer, time_offset + time_at_delta, (amp - ch->last_amp) << DELTA_VOL);
            ch->last_amp = amp;
        }

        u32 time_to_next = period - ch->pt;

        if (time_at_delta + time_to_next > (u32)clocks)
        {
            ch->pt += ((u32)clocks - time_at_delta);
            break;
        }

        time_at_delta += time_to_next;
        ch->st = (ch->st + 1) & 0x1F;
        ch->pt = 0;
    }
}

__inline static void nesApuSoundNoiseRenderBlipSlice(NESAPU_NOISE *ch, blip_t* blip_buffer, int clocks, int time_offset, bool is_muted)
{
    // Update LFSR
    u32 period = ch->wl; // Noise table period is already in NES CPU cycles
    if (is_muted || clocks <= 0 || period == 0)
    {
        return;
    }

    if (period < 4)
    {
        period = 4; // Safety clamp
    }

	// Silence Logic
    bool logical_mute = (ch->lc.counter == 0 || ch->mute);
    int current_vol = (logical_mute) ? 0 : (ch->ed.disable ? ch->ed.volume : ch->ed.counter);

    // Update LFSR and time only.
    if (current_vol == 0)
    {
        if (ch->last_amp != 0)
        {
            blip_add_delta_fast(blip_buffer, time_offset, -(ch->last_amp << DELTA_VOL));
            ch->last_amp = 0;
        }
        ch->pt = (ch->pt + clocks) % period;
        return;
    }

    // Volume gate render
    u32 time_at_delta = 0;
    // If ch->pt hangs with a high period value, fix it.
    if (ch->pt >= period)
    {
        ch->pt %= period;
    }

    while (time_at_delta < (u32)clocks)
    {
        u32 time_to_next = period - ch->pt;
        if (time_at_delta + time_to_next > (u32)clocks)
        {
            // We don't reach to the next change in this slice
            ch->pt += (clocks - time_at_delta);
            break;
        }

        // Advance up to the change
        time_at_delta += time_to_next;
        ch->pt = 0; // Reset phase

        // Spec: bit 0 XOR (bit 1 or bit 6, 15bit shift)
        u16 feedback = (ch->rng & 1) ^ ((ch->rng >> (ch->rngshort ? 6 : 1)) & 1);
        ch->rng = (ch->rng >> 1) | (feedback << 14);

        // New Amplitude
        int new_amp = (ch->rng & 1) ? 0 : current_vol;
        if (new_amp != ch->last_amp)
        {
            blip_add_delta_fast(blip_buffer, time_offset + time_at_delta, (new_amp - ch->last_amp) << DELTA_VOL);
            ch->last_amp = new_amp;
        }
    }
}

__inline static void nesApuSoundDmcRead(NESAPU_DPCM *ch)
{
    char ** memtbl = IPC_MEMTBL;
    // If the address exceeds 16 bits, wraps the range $8000-$FFFF
    if (ch->adr > 0xFFFF) 
    {
        ch->adr = 0x8000 + (ch->adr & 0x7FFF);
    }

    int addr = ch->adr;
    // (addr >> 13) - 4 converts $8000-$FFFF to indexes 0-3 for 8KB blocks
    ch->input = memtbl[(addr >> 13) - 4][addr & 0x1FFF];

    ch->adr++; 
}

__inline static void nesApuSoundDmcStart(NESAPU_DPCM *ch)
{
	ch->adr = 0xC000 | ((Uint16)ch->start_adr << 6);
    ch->length = ((Uint16)ch->start_length << 4) + 1; // Must be in bytes
    ch->bit_count = 0;
	ch->irq_report = 0;
	nesApuSoundDmcRead(ch);
}

// DS side NES Frame Counter Increments on each generated sample.
int raw_pcm_idx = 0;

// Called in the main loop, we need it to be reset each DS frame
void apuVblankSync()
{
    raw_pcm_idx = 0;
}

/**
 * @brief Frame-synchronized NES DMC ($4011) DAC write reconstruction.
 *
 * NES DMC raw PCM writes ($4011) require precise timing, but synchronizing
 * ARM9 and ARM7 in real time on the Nintendo DS is costly and unreliable.
 * Instead of forwarding each write through IPC or FIFO, ARM9 timestamps
 * each $4011 write using the current NES scanline and stores it in shared
 * IPC memory.
 *
 * ARM7 generates audio at the defined [DS_SOUND_FREQUENCY] rate and reconstructs the timing
 * by mapping each produced audio sample to a corresponding NES scanline
 * within the current frame. When a valid timestamped write is detected,
 * the DMC DAC output is updated at the correct point in the audio timeline.
 *
 * This method avoids tight CPU synchronization, is deterministic,
 * frame-perfect, and emulates the NES DMC DAC behavior despite
 * differing clocks between ARM9 and ARM7.
 */
inline static void nesApuReplayDmcPcmWrites(NESAPU_DPCM *ch)
{
	// RAW PCM samples must be rendered frame-perfect, hence this "async" method
	// This emulates RAW PCM sample fetching in DS speeds.

	unsigned char *raw_pcm_buffer = (unsigned char *)IPC_PCMDATA;
	// Sample Rate = Number of 32kHz samples that fit in 1/60 seconds.
	int samp_rate = SAMPLES_PER_DS_FRAME;
    
	// If for any reason the audio requests more than what we calculate in one frame
    // (samp_rate), we limit the index to avoid reading garbage
    int current_idx = raw_pcm_idx;
    if (current_idx >= samp_rate)
	{
		current_idx = samp_rate - 1;
	}
    int pcm_idx = (current_idx * NES_SCANLINES) / samp_rate;
    if (raw_pcm_buffer[pcm_idx] & 0x80) 
    {
        ch->dacout = raw_pcm_buffer[pcm_idx] & 0x7F;
        raw_pcm_buffer[pcm_idx] = 0; // Flush buffer after consume to avoid garbage leftovers
    }
	raw_pcm_idx++;
}

__inline static void nesApuSoundDmcRenderBlipSlice(NESAPU_DPCM *ch, blip_t* blip_buffer, int clocks, int time_offset, bool is_muted)
{
    #define ch (&apu.dpcm)
    if (clocks <= 0 || is_muted)
    {
        return;
    }

    u32 start_time = (u32)time_offset;
    u32 end_time = (u32)(time_offset + clocks);
    //PCM_Queue *q = IPC_PCM_QUEUE;

    // DPCM period table is already in NES CPU cycles
    u32 period = ch->wl ? ch->wl : 428;

    //--- SPECIAL DMC $4011 LOGIC --- (TODO: fix this) 
    //nesApuReplayDmcPcmWrites(ch);
    // while (q->tail != q->head)
    // {
    //     u32 event_cycle = q->events[q->tail].cycle;
    //     if (event_cycle < end_time)
    //     {
    //         int delta_time = (int)event_cycle - (int)start_time;
    //         if (delta_time < 0) delta_time = 0;

    //         u8 new_val = q->events[q->tail].value & 0x7F;
            
    //         if (new_val != ch->dacout)
    //         {
    //             blip_add_delta(blip_buffer, start_time + delta_time, (int)(new_val - ch->dacout) << DMC_DELTA_VOL);
    //             ch->dacout = new_val;
    //             ch->last_amp = new_val;
    //         }
            
    //         q->tail = (q->tail + 1) & PCM_QUEUE_MASK;
    //     }
    //     else
    //     {
    //         break;
    //     }
    // }

    // --- STANDARD DMC LOGIC ---
    
    // (Output Unit)
    // Only process if there are remaining bits in the current 8 bit cycle
    // and if we aren't muted
    if (ch->key && ch->length > 0)
    {
        u32 t = 0;
        while (t < (u32)clocks)
        {
            // Calculate next tick
            u32 time_to_next = period - (ch->pt % period);
            u32 next_t = t + time_to_next;
            if (next_t > (u32)clocks)
            {
                ch->pt += ((u32)clocks - t);
                break;
            }

            // Process Counter
            ch->pt += time_to_next;
            t = next_t;
            ch->pt -= period;

            int old_dac = ch->dacout;
            if (ch->input & 1)
            {
                if (ch->dacout <= 125)
                {
                    ch->dacout += 2;
                }
            }
            else
            {
                if (ch->dacout >= 2)
                {
                    ch->dacout -= 2;
                }
            }
            
            // Inyect delta if any changes ocurred
            if (ch->dacout != old_dac)
            {
                blip_add_delta_fast(blip_buffer, start_time + t, (int)(ch->dacout - old_dac) << DMC_DELTA_VOL);
                ch->last_amp = ch->dacout;
            }

            // Shift the register
            ch->input >>= 1;
            // Bit counter (8bit cycle per byte)
			// We use ch->bit_count to emulate the NES 8bit internal counter)
            ch->bit_count++;
            if (ch->bit_count >= 8)
            {
                ch->bit_count = 0;
                // Try to reload buffer from memory
                if (ch->length > 0)
                {
                    nesApuSoundDmcRead(ch); // Reads the next byte from the IPC channel
                    ch->length--; // Decrements remaining bytes
                    if (ch->length == 0)
                    {
                        if (ch->loop_enable)
                        { 
                            nesApuSoundDmcStart(ch); // Resets
                        }
                        else if (ch->irq_enable) 
                        {
                            apu.dpcm.irq_report |= APU_STATUS_DMC_IRQ; // Raises a DMC IRQ
                        }
                    }
                }
            }
        }
    }
    #undef ch
}

// Main blip_buf render function
void nesApuProcessBlipBufferChannels(int sample_count, s16* output_buffer)
{
    uint32_t nes_apu_clock = apu_cfg.region_pal ? NES_CPU_PAL : NES_CPU_NTSC;
    int total_clocks = blip_clocks_needed(master_blip, sample_count);
    if (total_clocks <= 0) return;

    int time_done = 0;
    // Handle the Frame Sequencer modes (4/5) properly
    bool mode5 = (apu.regs[0x17] & APU_FRAME_5STEP);

    while (time_done < total_clocks)
    {
        // Calculate how much time is left for the next APU "tick" (Envelope/Sweep/etc)
        // Select the proper frame sequencer table (AKA frame counter)
        const Uint32* current_seq = mode5 ? frame_seq_5 : frame_seq_4;
        int max_step = mode5 ? 5 : 4;
        
        uint32_t next_event = current_seq[apu.fp];
        int clocks_to_run = total_clocks - time_done;
        int clocks_until_next_tick = (int)next_event - (int)apu.fc;

        if (clocks_to_run > clocks_until_next_tick)
            clocks_to_run = clocks_until_next_tick;

        // RENDER SLICES
        nesApuSoundPulseRenderBlipSlice(&apu.square[0], master_blip, clocks_to_run, time_done, apu_cfg.pu1);
        nesApuSoundPulseRenderBlipSlice(&apu.square[1], master_blip, clocks_to_run, time_done, apu_cfg.pu2);
        nesApuSoundTriangleRenderBlipSlice(&apu.triangle, master_blip, clocks_to_run, time_done, apu_cfg.tri);
        nesApuSoundNoiseRenderBlipSlice(&apu.noise, master_blip, clocks_to_run, time_done, apu_cfg.noi);
        time_done += clocks_to_run;
        
        // CLOCK THE FRAME SEQUENCER FOR EVERY APU CHANNEL
        // https://www.nesdev.org/wiki/APU_Frame_Counter
        apu.fc += clocks_to_run;
        if (apu.fc >= next_event)
        {
            int step = apu.fp;
            if (mode5) // 5 Step mode
            {
                // Step 0: .ed | step 1: .ed .li | step 2: .ed | step 3: nothing | step 4: .ed .li
                if (step == 0 || step == 1 || step == 2 || step == 4)
                {
                    envelopeDecayStep(&apu.square[0].ed);
                    envelopeDecayStep(&apu.square[1].ed);
                    envelopeDecayStep(&apu.noise.ed);
                    linearCounterStepBlip(&apu.triangle.li);
                }
                if (step == 1 || step == 4)
                {
                    lengthCounterStep(&apu.square[0].lc);
                    lengthCounterStep(&apu.square[1].lc);
                    lengthCounterStep(&apu.triangle.lc);
                    lengthCounterStep(&apu.noise.lc);
                    sweepStep(&apu.square[0].sw, &apu.square[0].wl);
                    sweepStep(&apu.square[1].sw, &apu.square[1].wl);
                }
            }
            else // 4 Step mode:
            {
                // step 0, 1, 2, 3: .ed | step 1, 3: .li | step 3: apuirq
                envelopeDecayStep(&apu.square[0].ed);
                envelopeDecayStep(&apu.square[1].ed);
                envelopeDecayStep(&apu.noise.ed);
                linearCounterStepBlip(&apu.triangle.li);

                if (step == 1 || step == 3)
                {
                    lengthCounterStep(&apu.square[0].lc);
                    lengthCounterStep(&apu.square[1].lc);
                    lengthCounterStep(&apu.triangle.lc);
                    lengthCounterStep(&apu.noise.lc);
                    sweepStep(&apu.square[0].sw, &apu.square[0].wl);
                    sweepStep(&apu.square[1].sw, &apu.square[1].wl);
                }
                if (step == 3 && !(apu.regs[0x17] & APU_FRAME_IRQ_OFF))
                {
                    apuirq = 1; // Fire IRQ if disabled
                }
            }
            apu.fp++;
            if (apu.fp >= max_step)
            {
                apu.fp = 0;
                apu.fc = 0;
            }
            // --- UPDATE DS PSG PULSE HARDWARE IF ENABLED ---
            if (apu_cfg.hw_render)
            {
                nesApuSoundHwRender(nes_apu_clock);
            }
        }
    }

    // DMC is a sample channel, it doesn't need any counter update.
    nesApuSoundDmcRenderBlipSlice(&apu.dpcm, master_blip, total_clocks, 0, apu_cfg.dmc);

    // Mix everything
    blip_end_frame(master_blip, total_clocks);
    blip_read_samples(master_blip, output_buffer, sample_count, 0);
    ptr_mixed = 0; // Always reset read pointer
}

void apuSoundWrite(Uint address, Uint value)
{
	// NES APU REGISTERS ($4000 ~ $4017)
	if (address < 0x4018)
	{
	    apu.regs[address - APU_PULSE1_CTRL] = value;
        switch (address)
		{
			// Pulse ($4000–$4007)
			case APU_PULSE1_CTRL:
			case APU_PULSE2_CTRL:
			{
				int ch = (address >> 2) & 1;
				// VVVV: Constant volume / Envelope Rate
				apu.square[ch].ed.volume = value & PULSE_VOLUME_MASK;
				apu.square[ch].ed.rate   = value & PULSE_VOLUME_MASK;
				// C: Constant Volume flag
				apu.square[ch].ed.disable = (value >> 4) & 1; // PULSE_ENV_CONST_VOL
				// L: Halt length counter / Envelope Loop
				// IMPORTANT: This bit has a double function
				apu.square[ch].lc.clock_disable = (value >> 5) & 1; // PULSE_ENV_LOOP
				apu.square[ch].ed.looping_enable = (value >> 5) & 1; // PULSE_ENV_LOOP
				// Load the Duty Cycle from the table
				apu.square[ch].duty = square_duty_table[value >> 6];
				break;
			}
			// Sweep unit ($4001 / $4005)
			case APU_PULSE1_SWEEP:
			case APU_PULSE2_SWEEP:
			{
				int ch = (address >= APU_PULSE2_CTRL);
				apu.square[ch].sw.active    = (value >> 7); // PULSE_SWEEP_ENABLE
				apu.square[ch].sw.rate      = (value >> 4) & 7; // Bits 4-6 are the period
				apu.square[ch].sw.direction = (value >> 3) & 1; // PULSE_SWEEP_NEGATE
				apu.square[ch].sw.shifter   = value & PULSE_SWEEP_SHIFT;
				// Spec: Writing here marks the sweep for reload.
				apu.square[ch].sw.timer = 0; 
				break;
			}
			// Timer low ($4002 / $4006)
			case APU_PULSE1_TIMER_L:
			case APU_PULSE2_TIMER_L:
			{
				int ch = (address >> 2) & 1; // APU_PULSE2_CTRL
				apu.square[ch].wl = (apu.square[ch].wl & 0xFF00) | value; //PULSE_TIMER_LOW;
				break;
			}
			// Timer High + LC Load ($4003 / $4007)
			case APU_PULSE1_TIMER_H:
			case APU_PULSE2_TIMER_H:
			{
				int ch = (address >> 2) & 1;
				// Update timer (Wavelength)
				// We use timer high (bits 0-2) and preserve timer low
				apu.square[ch].wl = (apu.square[ch].wl & 0x00FF) | ((value & 7) << 8); // PULSE_TIMER_HIGH
				// Load the Length Counter from the table
				// If status bit (4015) is enabled for this channel, load it.
				apu.square[ch].lc.counter = (vbl_length_table[value >> 3]);
				// Side Effects (Spec):
				apu.square[ch].st = 0;           // "resets the phase of the pulse generator"
				apu.square[ch].ed.counter = PULSE_VOLUME_MASK;  // "restarts the envelope" (returns to max volume)
				apu.square[ch].ed.timer = 0;      // "resets the envelope divisor"
				
				break;
			}
			// Triangle ($4008–$400B)
			// Length counter halt, linear counter control/load ($4008)
			case APU_TRI_CTRL:
			{
				apu.triangle.li.load = value & TRI_LINEAR_LOAD;
				// C controls both: Length Halt and Linear Control flag
				apu.triangle.lc.clock_disable = (value & TRI_LINEAR_HALT) ? 1 : 0;
				apu.triangle.li.clock_disable = (value & TRI_LINEAR_HALT) ? 1 : 0;
				break;
			}
			// Timer Low ($400A)	
			case APU_TRI_TIMER_L:
			{
				apu.triangle.wl &= 0x0700; // Cleans lows, keeps highs (bits 8-10)
    			apu.triangle.wl |= value;
				break;
			}
			// Length counter load, timer high, set linear counter reload flag ($400B)
			case APU_TRI_TIMER_H:
			{
				apu.triangle.wl &= 0x00FF; // Cleans highs, keeps lows (bits 8-10)
				apu.triangle.wl |= ((value & TRI_TIMER_HIGH_MASK) << 8);
				// Loads Length Counter from the table
				apu.triangle.lc.counter = vbl_length_table[value >> 3];
				apu.triangle.li.tocount = 1; // Spec: "Secondary effect: Sets the linear counter reload flag"
				break;
			}
			// Noise ($400C–$400F)
			// Envelope loop/lenght counter halt/envelope ($400C)
			case APU_NOISE_CTRL:
			{
				if (value & NOISE_ENV_CONST)
				{
					apu.noise.ed.volume = value & NOISE_VOLUME_MASK;
				}
				else
				{
					apu.noise.ed.rate = value & NOISE_VOLUME_MASK;
				}
				apu.noise.ed.disable = value & NOISE_ENV_CONST;
				apu.noise.lc.clock_disable = value & NOISE_ENV_LOOP;
				apu.noise.ed.looping_enable = value & NOISE_ENV_LOOP;
				break;
			}
			// Loop noise/period ($400E)
			case APU_NOISE_PERIOD:
			{
				apu.noise.wl = noise_time_period_table[value & NOISE_VOLUME_MASK];
				apu.noise.rngshort = value & NOISE_MODE;
				break;
			}
			// Length counter load ($400F)
			case APU_NOISE_LENGTH:
			{
				// LLLL L--- (Bits 3-7)
				apu.noise.lc.counter = (vbl_length_table[value >> 3]);
				// Spec: Side effects
				apu.noise.ed.counter = 0xF; // Restart envelope
				apu.noise.ed.timer = 0;     // Reset envelope divider
				apu.noise.ed.start = 1;  // If we use a start flag
				break;
			}
			// DMC ($4010–$4013)
			// IRQ enable, loop, freq ($4010)
			case APU_DMC_CTRL:
			{
			    apu.dpcm.wl = dpcm_freq_table[value & DMC_RATE_MASK];
				apu.dpcm.loop_enable = value & DMC_LOOP;
				apu.dpcm.irq_enable = value & DMC_IRQ_ENABLE;
				if (!apu.dpcm.irq_enable)
				{
					apu.dpcm.irq_report = 0;
				}
				break;
			}
			// Load Counter ($4011)
			case APU_DMC_LOAD:
			{
				apu.dpcm.dacout = value & DMC_DAC_MASK;
				break;
			}
			// Sample address ($4012)
			case APU_DMC_ADDR:
			{
				apu.dpcm.start_adr = value;
				break;
			}
			// Sample length ($4013)
			case APU_DMC_LENGTH:
			{
				apu.dpcm.start_length = value;
				break;
			}
			// Status ($4015)
			// Write/Read ($4015)
			case APU_STATUS:
            {
				// Pulse 1
                apu.square[0].key = (value & 1);
                if (!apu.square[0].key)
				{
					apu.square[0].lc.counter = 0;
				}
				// Pulse 2
                apu.square[1].key = (value >> 1) & 1;
                if (!apu.square[1].key)
				{
 					apu.square[1].lc.counter = 0;
				}
				// Triangle
                apu.triangle.key = (value >> 2) & 1;
                if (!apu.triangle.key)
				{
                    apu.triangle.lc.counter = 0;
                    apu.triangle.li.counter = 0;
                }
				// Noise
                apu.noise.key = (value >> 3) & 1;
                if (!apu.noise.key)
				{
					apu.noise.lc.counter = 0;
				}
				// DMC
                if (value & APU_CH_DMC)
				{
                    if (!apu.dpcm.key || apu.dpcm.length == 0) // If not active or sample has ended
					{
                        apu.dpcm.key = 1;
                        nesApuSoundDmcStart(&apu.dpcm); // Process DCM data
                    }
                } 
				else 
				{
                    apu.dpcm.key = 0;
                    apu.dpcm.length = 0; // Stops the sample immediately
                }
                apu.dpcm.irq_report = 0; // Clean the IRQ flag when writing to $4015
                break;
			}
			// Frame Counter ($4017)	
			case APU_FRAME_COUNTER:
			{
                apu.regs[0x17] = value;
                apu.fc = 0;
                apu.fp = 0;
				if (value & APU_FRAME_5STEP)
                {
                    // Spec: "immediately clock all of its controlled units"
                    envelopeDecayStep(&apu.square[0].ed);
                    envelopeDecayStep(&apu.square[1].ed);
                    envelopeDecayStep(&apu.noise.ed);
                    linearCounterStepBlip(&apu.triangle.li);

                    lengthCounterStep(&apu.square[0].lc);
                    lengthCounterStep(&apu.square[1].lc);
                    lengthCounterStep(&apu.triangle.lc);
                    lengthCounterStep(&apu.noise.lc);
                    
                    sweepStep(&apu.square[0].sw, &apu.square[0].wl);
                    sweepStep(&apu.square[1].sw, &apu.square[1].wl);
				}
                // If bit 6 is set, clean the IRQ flag
                if (value & APU_FRAME_IRQ_OFF)
                {
                    apuirq = 0;
                }
                break;
			}
		}
		return;
	}
	// FDS (FAMICOM DISK SYSTEM ADDITIONAL CHANNEL) TODO: REFACTOR WITH CASES
	if (has_fds && address >= FDS_BASE && address < FDS_END) 
	{
		FDSSoundWrite(address, value);
	}
	// VRC6 (KONAMI SOUND CHIP)
	else if (has_vrc6 && address >= VRC6_MIN_BASE)
	{
		switch (address & 0xF000)
		{
			// Pulse Control ($9000,$A000)
			case VRC6_PULSE1_CTRL:
			{
				if (address < 0x9003)
				{
					VRC6SoundWrite9000(address, value);
				}
				break;
			}
			case VRC6_PULSE2_CTRL:
			{
				if (address < 0xA003)
				{
					VRC6SoundWriteA000(address, value);
				}
				break;
			}
			// Saw Accum Rate ($B000)
			case VRC6_SAW_RATE:
			{
				if (address < 0xB003)
				{
					VRC6SoundWriteB000(address, value);
				}
				break;
			}
		}
	}
}

// ($4015) Channel enable and length counter status
void __fastcall APU4015Reg()
{
    static int oldkey = 0;
    
    // Evaluate channels status using bools instead
    int p1 = (apu.square[0].key && apu.square[0].lc.counter);
    int p2 = (apu.square[1].key && apu.square[1].lc.counter);
    int tri = (apu.triangle.key && apu.triangle.lc.counter && apu.triangle.li.counter);
    int noi = (apu.noise.key    && apu.noise.lc.counter);
    int dmc = (apu.dpcm.length > 0);

    // Construct the key
    // p1 is bit 0, p2 is bit 1...
    int key = p1 | (p2 << 1) | (tri << 2) | (noi << 3) | (dmc << 4);
    
    // Add interrupt flags
    key |= APU_STATUS_FRAME_IRQ | apu.dpcm.irq_report;

    // Only update IPC if something really changed or there are pending irqs
    if (oldkey != key || apuirq) 
    {
        IPC_REG4015 = key;
        IPC_APUIRQ = apuirq;
        oldkey = key;
        apuirq = 0;
    }
}

// Update APU Status flags only when the APU resets
static void apuSyncConfigCache(bool region_flag)
{
	// Set per-region table pointers
	square_duty_table = apu_cfg.duty_reverse
        ? square_duty_table_inverted
        : square_duty_table_normal;

    noise_time_period_table = region_flag
        ? noise_time_period_table_pal
        : noise_time_period_table_ntsc;

    dpcm_freq_table = region_flag
        ? dpcm_freq_table_pal
        : dpcm_freq_table_ntsc;

    // Set Frame Sequencer Tables
    if (region_flag)
    {
        frame_seq_4 = frame_seq_pal_4;
        frame_seq_5 = frame_seq_pal_5;
    }
    else
    {
        frame_seq_4 = frame_seq_ntsc_4;
        frame_seq_5 = frame_seq_ntsc_5;
    }
}

// We no longer need cps calculations now, since blip doesn't render per-sample
static void __fastcall apuSoundReset(void)
{
	// Set APU flags
	uint32_t nes_apu_clock = apu_cfg.region_pal ? NES_CPU_PAL : NES_CPU_NTSC;
	apuSyncConfigCache(apu_cfg.region_pal);

	// blip_buf is now in charge of the NES -> DS rates.
	nesApuBlipInit(nes_apu_clock, DS_SOUND_FREQUENCY);
    nesApuSoundHwStop();

	// Clear and Configure every APU channel and regs
	memset(&apu, 0, sizeof(APUSOUND));
    apu.noise.rng = 1; // Noise channel must be inited with 1
    apu.dpcm.first = 1;
    apu.regs[0x17] = 0x00; // Spec: Init $4017 reg en 4 step mode (0x00)

	for (int i = 0; i <= 0x17; i++)
	{
		apuSoundWrite(0x4000 + i, (i == 0x10) ? 0x10 : 0x00);
	}

	apuSoundWrite(0x4015, 0x0f);
	apu.dpcm.first = 1;
}

static NES_RESET_HANDLER s_apu_reset_handler[] = {
	{ NES_RESET_SYS_NOMAL, apuSoundReset, 0},
	{ 0,                   0, 0}
};

void apuSoundInstall(void)
{
	NESResetHandlerInstall(s_apu_reset_handler);
}
