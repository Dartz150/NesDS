#include <string.h>
#include "nestypes.h"
#include "audiosys.h"
#include "handler.h"
#include "nsf6502.h"
#include "nsdout.h"
#include "s_fds.h"

/* DS Mixer Compatibility Constants */
#define FDS_MIX_FACTOR 1 // Keep it like this to avoid sound overflows
#define FM_DEPTH 1
#define PGCPS_BITS (32 - 16 - 6)
#define EGCPS_BITS (12)

typedef struct {
	Uint8 spd;
	Uint8 cnt;
	Uint8 mode;
	Uint8 volume;
} FDS_EG;
typedef struct {
	Uint32 spdbase;
	Uint32 spd;
	Uint32 freq;
} FDS_PG;
typedef struct {
	Uint32 phase;
	Int8 wave[0x40];
	Uint8 wavptr;
	Int8 output;
	Uint8 disable;
	Uint8 disable2;
} FDS_WG;
typedef struct {
	FDS_EG eg;
	FDS_PG pg;
	FDS_WG wg;
	Uint8 bias;
	Uint8 wavebase;
	Uint8 d[2];
} FDS_OP;

typedef struct FDSSOUND_tag {
	FDS_OP op[2];
	Uint32 phasecps;
	Uint32 envcnt;
	Uint32 envspd;
	Uint32 envcps;
	Uint8 envdisable;
	Uint8 d[3];
	Uint32 lvl;
	Int32 mastervolumel[4];
	Uint32 mastervolume;
	Uint32 srate;
	Uint8 mod_table[32];
    Uint8 mod_pos;
} FDSSOUND;

static FDSSOUND fdssound;

static void FDSSoundEGStep(FDS_EG *peg) 
{
    if (peg->mode & 0x80) return; // Fixed volume mode
    if (peg->spd == 0) return;

    if (++peg->cnt <= peg->spd) return;
    peg->cnt = 0;

    if (peg->mode & 0x40) 
	{
        if (peg->volume < 0x1F) peg->volume++;
    } 
	else 
	{
        if (peg->volume > 0) peg->volume--;
    }
}

Int32 __fastcall FDSSoundRender(void) {
    Int32 output;
       // This prevents short "click" sounds and keeps correct note duration.
    if (!fdssound.envdisable && fdssound.envspd) 
	{
        fdssound.envcnt += fdssound.envcps;
        while (fdssound.envcnt >= fdssound.envspd) 
		{
			// Wave Generator
            fdssound.envcnt -= fdssound.envspd;
            FDSSoundEGStep(&fdssound.op[1].eg);
            FDSSoundEGStep(&fdssound.op[0].eg);
        }
    }

    // Modulator (op[1]) is processed first to affect Carrier (op[0]) pitch.
    if (fdssound.op[1].wg.disable || fdssound.op[1].wg.disable2)
	{
        fdssound.op[1].wg.output = 0;
	}
    else
    {
        fdssound.op[1].wg.output =
			fdssound.op[1].wg.wave[(fdssound.op[1].wg.phase >> (PGCPS_BITS + 16)) & 0x3f];
	}
    if (fdssound.op[0].wg.disable || fdssound.op[0].wg.disable2)
	{
        fdssound.op[0].wg.output = 0;
	}
    else
	{
        fdssound.op[0].wg.output =
			fdssound.op[0].wg.wave[(fdssound.op[0].wg.phase >> (PGCPS_BITS + 16)) & 0x3f];
	}
    // Frequency Modulation Logic
    // Balance achieved by using 0x10000 base with 12-bit wrapping.
    fdssound.op[1].pg.spd = fdssound.op[1].pg.spdbase;
    
    if (fdssound.op[1].wg.disable)
	{
        fdssound.op[0].pg.spd = fdssound.op[0].pg.spdbase;
    }
	else
	{
        Uint32 v1;
        // Bias and Wave Output combined to shift the Carrier frequency
        v1 = 0x10000 + ((Int32)fdssound.op[1].eg.volume) * (((Int32)((((Uint8)fdssound.op[1].wg.output)
			+ fdssound.op[1].bias) & 255)) - 64);
        v1 = ((1 << 10) + v1) & 0xfff;
        v1 = (fdssound.op[0].pg.freq * v1) >> 10;
        fdssound.op[0].pg.spd = v1 * fdssound.phasecps;
    }

    // Final Accumulator
    output = fdssound.op[0].eg.volume;
    if (output > 0x20) 
	{
		output = 0x20;
	}
    output = (fdssound.op[0].wg.output * output);

    // Phase Update
    fdssound.op[1].wg.phase += fdssound.op[1].pg.spd;
    fdssound.op[0].wg.phase += fdssound.op[0].pg.spd;

    // DS MIXER COMPATIBILITY LOGIC
    // Output is scaled using FDS master volume registers ($4089)
    // and bit-shifted to fit the DS audio mixer headroom.
    static const Uint8 master_scales[4] = {30, 20, 15, 12};
    Int32 final_out = (output * master_scales[fdssound.lvl & 3]) / 10;
    
    // Final output normalized for the DS emulated APU Mixer (1.5x)
    return (fdssound.op[0].pg.freq != 0) ? ((final_out * 3 ) >> FDS_MIX_FACTOR) : 0;
}

static const Uint8 wave_delta_table[8] = {
	0,(1 << FM_DEPTH),(2 << FM_DEPTH),(4 << FM_DEPTH),
	0,256 - (4 << FM_DEPTH),256 - (2 << FM_DEPTH),256 - (1 << FM_DEPTH),
};

// Called in s_apu.c
static void __fastcall FDSSoundWrite(Uint address, Uint value)
{
    if (0x4040 <= address && address <= 0x407F)
	{
        // Save bipolar for the render (-32 a 31)
        fdssound.op[0].wg.wave[address - 0x4040] = (Int8)((value & 0x3F) - 0x20);
        return;
    }
    FDS_OP *pop = &fdssound.op[(address & 4) >> 2];
    switch (address & 0x0F)
	{
        case 0: // Volume / Envelope
		case 4:
		{
            pop->eg.mode = value & 0xC0;
            if (pop->eg.mode & 0x80) pop->eg.volume = (value & 0x3F);
            else pop->eg.spd = value & 0x3F;
            break;
		}
		case 5: // $4085
		{
			fdssound.op[1].bias = value & 255;
			break;
		}
        case 2:
		case 6: // Pitch Low
		{
            pop->pg.freq = (pop->pg.freq & 0x0F00) | (value & 0xFF);
            pop->pg.spdbase = pop->pg.freq * fdssound.phasecps;
            break;
		}
        case 3: // Wave Pitch High / Halt ($4083)
		{
            pop->pg.freq = (pop->pg.freq & 0x00FF) | ((value & 0x0F) << 8);
            pop->pg.spdbase = pop->pg.freq * fdssound.phasecps;

            pop->wg.disable = value & 0x80;     // Bit 7: Wave Halt
            fdssound.envdisable = value & 0x40; // Bit 6: Env Halt
            
            if (pop->wg.disable)
			{
                pop->wg.phase = 0;
            }
            break;
		}
		case 7: // Mod Pitch High / Halt ($4087)
		{
            pop->pg.freq = (pop->pg.freq & 0x00FF) | ((value & 0x0F) << 8);
            pop->pg.spdbase = pop->pg.freq * fdssound.phasecps;

            pop->wg.disable = value & 0x80; // Bit 7: Mod Halt
            // Note: Bit 6 isn't used here per the spec

            if (pop->wg.disable)
			{
                pop->wg.phase = 0;
            }
            break;
		}
        case 8: // Mod Table Write ($4088)
		{
			if (fdssound.op[1].wg.disable)
			{
				Int32 idx = value & 7;
				if (idx == 4)
				{
					fdssound.op[1].wavebase = 0;
				}
				// FDS hardware translates the 3 bits to increments in the table
				fdssound.op[1].wavebase += wave_delta_table[idx];
				fdssound.op[1].wg.wave[fdssound.op[1].wg.wavptr + 0] = (fdssound.op[1].wavebase + 64) & 255;
				fdssound.op[1].wavebase += wave_delta_table[idx];
				fdssound.op[1].wg.wave[fdssound.op[1].wg.wavptr + 1] = (fdssound.op[1].wavebase + 64) & 255;
				fdssound.op[1].wg.wavptr = (fdssound.op[1].wg.wavptr + 2) & 0x3f;
			}
			break;
		}
        case 9: // Master Vol
		{
            fdssound.lvl = value & 3;
            fdssound.op[0].wg.disable2 = value & 0x80;
            break;
		}
		case 10: // $408A: Env Speed
		{
			// Keep the value directly for the Render comparator
			// We use the shift to escalate it to the counter precision
			fdssound.envspd = (value + 1) << EGCPS_BITS;
			break;
		}
    }
}

static void __fastcall FDSSoundReseter(FDSSOUND *ch) {
    XMEMSET(&fdssound, 0, sizeof(FDSSOUND));
    uint32_t sample_rate = NESAudioFrequencyGet();
    uint32_t cpu_clock = (getApuCurrentRegion() == PAL) ? NES_CPU_PAL : NES_CPU_NTSC;

    fdssound.phasecps = GetFixedPointStep(cpu_clock, sample_rate, PGCPS_BITS);

	// Global Envelope Speed Correction
    // Dividing cpu_clock by 8 provides the correct tick density for the DS,
    // matching the real FDS hardware slow envelope decays.
    fdssound.envcps = GetFixedPointStep(cpu_clock >> 3, sample_rate, EGCPS_BITS);
    fdssound.envspd = 0xe8 << EGCPS_BITS;
    fdssound.envdisable = 1;

    for (int i = 0; i < 0x40; i++)
	{
        fdssound.op[0].wg.wave[i] = (i < 0x20) ? 0x1F : -0x20;
    }
}

static void FDSSoundReset(void)
{
    FDSSoundReseter(&fdssound);
}

static NES_RESET_HANDLER s_fds_reset_handler[] =
{
	{ NES_RESET_SYS_NOMAL, FDSSoundReset, },
	{ 0,                   0, },
};

extern void FDSSoundInstall(void)
{
	FDSSoundWriteHandler = FDSSoundWrite;
	NESResetHandlerInstall(s_fds_reset_handler);
}
