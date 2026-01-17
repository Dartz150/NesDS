#include <nds.h>
#include <string.h>
#include "nestypes.h"
#include "audiosys.h"
#include "handler.h"
#include "nsf6502.h"
#include "nsdout.h"
#include "s_apu.h"
#include "c_defs.h"
#include "s_vrc6.h"
#include "s_apu_defs.h"

/* ------------------------- */
/*  NES INTERNAL SOUND(APU)  */
/* ------------------------- */

// Based from documentation found in https://www.nesdev.org/wiki/APU

/*/ Lenght Counter /*/
// Provides automatic duration control for the NES APU waveform channels ($4015 ~ $400F)
static int apuirq = 0;

typedef struct 
{
	Uint32 counter;			/* length counter */
	Uint8 clock_disable;	/* length counter clock disable ($4015) */
} LENGTHCOUNTER;

// Linear Counter
typedef struct 
{
	Uint32 cpf;				/* cycles per frame (240Hz fix) */
	Uint32 fc;				/* frame counter; */
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
	Uint32 cps;				/* cycles per sample */
	Uint32 *cpf;			/* cycles per frame (240/192Hz) ($4017.bit7) */
	Uint32 fc;				/* frame counter; */
	Uint32 wl;				/* wave length */
	Uint32 pt;				/* programmable timer */
	Uint8 st;				/* wave step */
	Uint8 fp;				/* frame position */
	Uint8 duty;				/* duty rate */
	Uint8 key;
	Uint8 mute;
} NESAPU_SQUARE;

typedef struct 
{
	LENGTHCOUNTER lc;		/* lenght counter */
	LINEARCOUNTER li;		/* linear counter */
	Uint32 mastervolume;	/* master volume (0x0 ~ +0x3FF) */
	Uint32 cps;				/* cycles per sample */
	Uint32 *cpf;			/* cycles per frame (240/192Hz) ($4017.bit7) */
	Uint32 fc;				/* frame counter; */
	Uint32 wl;				/* wave length */
	Uint32 pt;				/* programmable timer */
	Uint8 st;				/* wave step */
	Uint8 fp;				/* frame position; */
	Uint8 key;
	Uint8 mute;
} NESAPU_TRIANGLE;

typedef struct 
{
	LENGTHCOUNTER lc;
	LINEARCOUNTER li;
	ENVELOPEDECAY ed;
	Uint32 mastervolume;
	Uint32 cps;				/* cycles per sample */
	Uint32 *cpf;			/* cycles per frame (240/192Hz) ($4017.bit7) */
	Uint32 fc;				/* frame counter; */
	Uint32 wl;				/* wave length */
	Uint32 pt;				/* programmable timer */
	Uint32 rng;
	Uint8 rngshort;
	Uint8 fp;				/* frame position; */
	Uint8 key;
	Uint8 mute;
} NESAPU_NOISE;

typedef struct 
{
	Uint32 cps;				/* cycles per sample */
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
	Uint8 dacbase;
	Uint8 key;
	Uint8 mute;
} NESAPU_DPCM;

typedef struct 
{
	NESAPU_SQUARE square[2];
	NESAPU_TRIANGLE triangle;
	NESAPU_NOISE noise;
	NESAPU_DPCM dpcm;
	Uint32 cpf[3];			/* cycles per frame (240/192Hz) ($4017.bit7) */
	Uint8 regs[0x20];
} APUSOUND __attribute__((aligned(32)));

static APUSOUND apu;

// Square Duty LUT
static const Uint8 square_duty_table[4] = 
{ 
	0x02, 0x04, 0x08, 0x0C
};

static const Uint8 inverted_square_duty_table[4] = 
{ 
	0x0C, 0x08, 0x04, 0x02
};

// APU_Length_Counter LUT ($400F)
static const Uint8 vbl_length_table[32] = 
{
	0x0A, 0xFE, 0x14, 0x02, 0x28, 0x04, 0x50, 0x06,
	0xA0, 0x08, 0x3C, 0x0A, 0x0E, 0x0C, 0x1A, 0x0E,
	0x0C, 0x10, 0x18, 0x12, 0x30, 0x14, 0x60, 0x16,
	0xC0, 0x18, 0x48, 0x1A, 0x10, 0x1C, 0x20, 0x1E
};

// APU Noise Time Period LUT NTSC ($400E)
static const Uint32 noise_time_period_table_ntsc[16] =
{
	0x004, 0x008, 0x010, 0x020, 0x040, 0x060, 0x080, 0x0A0,
	0x0CA, 0x0FE, 0x17C, 0x1FC, 0x2FA, 0x3F8, 0x7F2, 0xFE4
};

//TODO: APU Noise Time Period LUT PAL ($400E)
static const Uint32 noise_time_period_table_pal[16] =
{
    0x004, 0x008, 0x00E, 0x01E, 0x03C, 0x058, 0x076, 0x094,
    0x0BC, 0x0EC, 0x162, 0x1D8, 0x2C4, 0x3B0, 0x762, 0xEC2
};

// APU DMC LUT NTSC ($4010)
static const Uint32 dpcm_freq_table_ntsc[16] =
{
	0x1AC, 0x17C, 0x154, 0x140, 0x11E, 0x0FE, 0x0E2, 0x0D6,
	0x0BE, 0x0A0, 0x08E, 0x080, 0x06A, 0x054, 0x048, 0x036
};

// TODO: APU DMC LUT PAL
static const Uint32 dpcm_freq_table_pal[16] =
{
	0x18E, 0x162, 0x13C, 0x12A, 0x114, 0x0EC, 0x0D2, 0x0C6,
	0x0B0, 0x094, 0x084, 0x076, 0x062, 0x04E, 0x042, 0x032
};

inline static void LengthCounterStep(LENGTHCOUNTER *lc)
{
	if (lc->counter && !lc->clock_disable) 
	{
		lc->counter--;
	}
}

inline static void LinearCounterStep(LINEARCOUNTER *li, Uint32 cps)
{
	li->fc += cps;
    while (li->fc >= li->cpf)
    {
        li->fc -= li->cpf;

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
}

inline static void EnvelopeDecayStep(ENVELOPEDECAY *ed)
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

inline void SweepStep(SWEEP *sw, Uint32 *wl)
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

static Int32 NESAPUSoundSquareRender(NESAPU_SQUARE *ch)
{
	Int32 output;
	if (!ch->key || !ch->lc.counter)
	{
		return 0;
	}
	// Frame Counter and sequencer (Envelopes, Sweeps, Length)
    ch->fc += ch->cps;
    while (ch->fc >= *(ch->cpf))
    {
		ch->fc -= *(ch->cpf);
		// A real NES updates at 60, 120 y 240Hz respectively
		if (!(ch->fp & 3))
		{
			LengthCounterStep(&ch->lc);	  // 60Hz
		}
		if (!(ch->fp & 1))
		{
			SweepStep(&ch->sw, &ch->wl);  // 120Hz 
		}
		EnvelopeDecayStep(&ch->ed);       // 240Hz
		ch->fp++;
	}
    // Verify NES hardware limits
    // Sweep silences the channel if the WL is > 0x7FF or < 8
	Uint32 sweep_target = ch->wl + (ch->wl >> ch->sw.shifter);
	if (ch->wl < 8 || (!ch->sw.direction && sweep_target > 0x7FF)) 
	{
		return 0;
	}
	ch->pt += ch->cps;
	
	// https://www.nesdev.org/wiki/APU#Pulse_($4000%E2%80%93$4007)
	// f = fCPU / (16 × (t + 1))
	Uint32 period = (ch->wl + 1) << CPS_SHIFT;
	while (ch->pt >= period)
	{
		ch->pt -= period;
		ch->st = (ch->st + 1) & PULSE_VOLUME_MASK; // 16 steps cycle
	}
	if (ch->mute) 
	{
		return 0;
	}
	// Generate amplitude
    output = ch->ed.disable
		? ch->ed.volume
		: ch->ed.counter;
	return (ch->st >= ch->duty)
		? -output
		: output;
}

Int32 NESAPUSoundSquareRender1()
{
	return NESAPUSoundSquareRender(&apu.square[0]);
}

Int32 NESAPUSoundSquareRender2()
{
	return NESAPUSoundSquareRender(&apu.square[1]);
}

static Int32 NESAPUSoundTriangleRender(NESAPU_TRIANGLE *ch)
{
	// Update timers (Linear -> 240Hz, Length -> 60Hz)
	LinearCounterStep(&ch->li, ch->cps); // 240Hz
	ch->fc += ch->cps;
	while (ch->fc >= *(ch->cpf))
	{
		ch->fc -= *(ch->cpf);
		if (!(ch->fp & 3)) 
		{
			LengthCounterStep(&ch->lc);	// 60Hz
		}
		ch->fp++;
	}

	// Avoid aliassing with too high frequencies
    if (ch->wl < 2) 
	{
		return 0;
	}

	// Spec: The sequencer only advances if BOTH are more than zero.
	//      Linear Counter   Length Counter
	//             |                |
	//             v                v
	// Timer ---> Gate ----------> Gate ---> Sequencer ---> (to mixer)
	if (ch->lc.counter > 0 && ch->li.counter > 0 && ch->wl >= 2)
    {
		// Oscilator (Triangle runs at twice the speed of the squares)
		// Real period IS WL + 1
		ch->pt += ch->cps;
		Uint32 period = (ch->wl + 1) << CPS_SHIFT;
		while (ch->pt >= period)
		{
			ch->pt -= period;
			ch->st = (ch->st + 1) & 0x1F; // 32 step cycle (0-31)
		}
	}

	if (ch->mute) 
	{
		return 0;
	}
	// Wave Generation
	Int32 output = ch->st;
    if (output & 0x10)
	{
		output = 0x1F - output; // Invert to create the slope
	}
	// IMPORTANT: Return the real unsigned value (0-15)
    // NEVER invert the sign using -output.
	return output - 8;
}

Int32 NESAPUSoundTriangleRender1()
{
	return NESAPUSoundTriangleRender(&apu.triangle);
}

static Int32 NESAPUSoundNoiseRender(NESAPU_NOISE *ch)
{	
	// Frame Counter and sequencer (Envelope, Length)
	ch->fc += ch->cps;
	while (ch->fc >= *(ch->cpf))
	{
		ch->fc -= *(ch->cpf);
		if (!(ch->fp & 3))
		{
			LengthCounterStep(&ch->lc);	/* 60Hz */
		}
		EnvelopeDecayStep(&ch->ed); 	/* 240Hz */
		ch->fp++;
	}
	// Silence Logic
	if (ch->lc.counter == 0)
	{
		return 0;
	}
	// Update LFSR
	ch->pt += ch->cps;
	Uint32 period = ch->wl << CPS_SHIFT; // Noise table period is already in NES CPU cycles
	if (period == 0) 
	{
		return 0;
	}

	while (ch->pt >= period)
	{
		ch->pt -= period;
		// Spec: bit 0 XOR (bit 1 or bit 6, 15bit shift)
		Uint8 feedback = (ch->rng & 1) ^ ((ch->rng >> (ch->rngshort ? 6 : 1)) & 1);
		ch->rng = (ch->rng >> 1) | (feedback << 14);
	}

	if (ch->mute)
	{
		return 0;
	}

	// Volume gate
	// If bit 0 is 1, silence the channel. 
	// If is 0, let volume pass.
	if (ch->rng & 1) 
	{ 
		return 0;
	}

	Uint8 vol = ch->ed.disable 
		? ch->ed.volume 
		: ch->ed.counter;
	
	// Center volume (0-15 -> -8-7)
	// For some reason yet unknown, vol -8 produces too much noise, so we leave vol alone
    return (Int32)vol;
}

Int32 NESAPUSoundNoiseRender1()
{
	return NESAPUSoundNoiseRender(&apu.noise);
}

__inline static void NESAPUSoundDpcmRead(NESAPU_DPCM *ch)
{
	char ** memtbl = IPC_MEMTBL;
	int addr = ch->adr;
	ch->input = memtbl[(addr >> 13) - 4][addr & 0x1FFF];
	ch->adr++;
}

static void NESAPUSoundDpcmStart(NESAPU_DPCM *ch)
{
	ch->adr = 0xC000 + ((Uint)ch->start_adr << 6);
	ch->length = (((Uint)ch->start_length << 4) + 1) << 3;
	ch->irq_report = 0;
	NESAPUSoundDpcmRead(ch);
}

static Int32 __fastcall NESAPUSoundDpcmRender(void)
{
	#define ch (&apu.dpcm)

	if (ch->first)
	{
		ch->first = 0;
		ch->dacbase = ch->dacout;
	}
	if (ch->key && ch->length)
	{
		ch->pt += ch->cps;
		// DPCM period table is already in NES CPU cycles
		Uint32 period = ch->wl << CPS_SHIFT;
		while (ch->pt >= period)
		{
			ch->pt -= period;
			if (ch->length == 0)
			{
				continue;
			}
			if (ch->input & 1)
			{
				ch->dacout += (ch->dacout < + DMC_DAC_OUT);
			}
			else
			{
				ch->dacout -= (ch->dacout > 0);
			}
			ch->input >>= 1;
			if (--ch->length == 0)
			{
				if (ch->loop_enable)
				{
					NESAPUSoundDpcmStart(ch);	/*loop */
				}
				else
				{
					if (ch->irq_enable)
					{
						//NES6502Irq();	/* irq gen */
						apuirq = DMC_DAC_RELOAD;
						ch->irq_report = APU_STATUS_DMC_IRQ;
					}
					ch->length = 0;
				}
			}
			else if ((ch->length & 7) == 0)
			{
				NESAPUSoundDpcmRead(ch);
			}
		}
		if (ch->mute) 
		{
			return 0;
		}
		// Return the centered value (Range 0-127 -> -64 a 63)
		return ((ch->dacout << 1) + ch->dacout0 - DMC_LOOP);
	}
	return 0;
	#undef ch
}

Int32 NESAPUSoundDpcmRender1()
{
	return NESAPUSoundDpcmRender();
}

void APUSoundWrite(Uint address, Uint value)
{
	int mapper = IPC_MAPPER;

	// NES APU REGISTERS ($4000 ~ $4017)
	if (APU_PULSE1_CTRL <= address && address <= APU_FRAME_COUNTER)
	{
		//if (NSD_out_mode && address <= 0x4015) NSDWrite(NSD_APU, address, value);
	    apu.regs[address - APU_PULSE1_CTRL] = value;
        switch (address)
		{
			// Pulse ($4000–$4007)
			case APU_PULSE1_CTRL:
			case APU_PULSE2_CTRL:
			{
				int ch = address >= APU_PULSE2_CTRL;
				if (value & PULSE_ENV_CONST_VOL)
				{
					apu.square[ch].ed.volume = value & PULSE_VOLUME_MASK;
				}
				else
				{
					apu.square[ch].ed.rate   = value & PULSE_VOLUME_MASK;
				}
				apu.square[ch].ed.disable = value & PULSE_ENV_CONST_VOL;
				apu.square[ch].lc.clock_disable = value & PULSE_ENV_LOOP;
				apu.square[ch].ed.looping_enable = value & PULSE_ENV_LOOP;
				if (getApuCurrentStatus() == Reverse)
				{
					apu.square[ch].duty = inverted_square_duty_table[value >> 6];
				}
				else 
				{
					apu.square[ch].duty = square_duty_table[value >> 6];
				}
				break;
			}
			// Sweep unit ($4001 / $4005)
			case APU_PULSE1_SWEEP:
			case APU_PULSE2_SWEEP:
			{
				int ch = address >= APU_PULSE2_CTRL;
				apu.square[ch].sw.shifter = value & PULSE_SWEEP_SHIFT;
				apu.square[ch].sw.direction = value & PULSE_SWEEP_NEGATE;
				apu.square[ch].sw.rate = (value >> 4) & PULSE_SWEEP_SHIFT;
				apu.square[ch].sw.active = value & PULSE_SWEEP_ENABLE;
				apu.square[ch].sw.timer = 0;
				break;
			}
			// Timer low ($4002 / $4006)
			case APU_PULSE1_TIMER_L:
			case APU_PULSE2_TIMER_L:
			{
				int ch = address >= APU_PULSE2_CTRL;
				apu.square[ch].wl &= PULSE_TIMER_LOW;
				apu.square[ch].wl += value;
				break;
			}
			// Timer High ($4003 / $4007)
			case APU_PULSE1_TIMER_H:
			case APU_PULSE2_TIMER_H:
			{
				int ch = address >= APU_PULSE2_CTRL;
				// apu.square[ch].pt = 0;
	#if 1
				apu.square[ch].st = 0;
	#endif
				apu.square[ch].wl &= PULSE_LENGTH_RELOAD;
				apu.square[ch].wl += (value & PULSE_TIMER_HIGH) << 8;
				apu.square[ch].ed.counter = 0xF;
				apu.square[ch].lc.counter = (vbl_length_table[value >> 3]) >> 1;
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
				apu.triangle.wl |= (value & TRI_TIMER_HIGH_MASK) << 8;
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
				if (getApuCurrentRegion() == PAL)
				{
					apu.noise.wl = (noise_time_period_table_pal[value & NOISE_VOLUME_MASK]);
				} 
				else 
				{
					apu.noise.wl = (noise_time_period_table_ntsc[value & NOISE_VOLUME_MASK]);
				}
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
				if (getApuCurrentRegion() == PAL)
				{
					apu.dpcm.wl = dpcm_freq_table_pal[value & DMC_RATE_MASK];
				}
				else
				{
					apu.dpcm.wl = dpcm_freq_table_ntsc[value & DMC_RATE_MASK];
				}
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
	#if 0
				if (apu.dpcm.first && (value & DMC_DAC_MASK))
				{
					apu.dpcm.first = 0;
					apu.dpcm.dacbase = value & DMC_DAC_MASK;
				}
	#endif
				apu.dpcm.dacout = (value >> 1) & DMC_DAC_OUT;
				apu.dpcm.dacbase = value & DMC_DAC_MASK;
				apu.dpcm.dacout0 = value & 1;
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
				if (value & APU_CH_PULSE1)
				{
					apu.square[0].key = 1;
				}
				else
				{
					apu.square[0].key = 0;
					apu.square[0].lc.counter = 0;
				}
				if (value & APU_CH_PULSE2)
				{
					apu.square[1].key = 1;
				}
				else
				{
					apu.square[1].key = 0;
					apu.square[1].lc.counter = 0;
				}
				if (value & APU_CH_TRIANGLE)
				{
					apu.triangle.key = 1;
				}
				else
				{
					apu.triangle.key = 0;
					apu.triangle.lc.counter = 0;
					apu.triangle.li.counter = 0;
					apu.triangle.li.mode = 0;
				}
				if (value & APU_CH_NOISE)
				{
					apu.noise.key = 1;
				}
				else
				{
					apu.noise.key = 0;
					apu.noise.lc.counter = 0;
				}
				if (value & APU_CH_DMC)
				{
					if (!apu.dpcm.key)
					{
						apu.dpcm.key = 1;
						NESAPUSoundDpcmStart(&apu.dpcm);
					}
				}
				else
				{
					apu.dpcm.key = 0;
				}
				break;
			}
			// Frame Counter ($4017)	
			case APU_FRAME_COUNTER:
			{
				if (value & APU_FRAME_5STEP)
				{
					apu.cpf[0] = apu.cpf[2];
				}
				else
				{
					apu.cpf[0] = apu.cpf[1];
				}
				break;
			}
		}
	}
	// FDS (FAMICOM DISK SYSTEM ADDITIONAL CHANNEL) TODO: REFACTOR WITH CASES
	else if (FDS_BASE <= address && address < FDS_END && (mapper == 20 || mapper == 256)) 
	{
		FDSSoundWriteHandler(address, value);
	}
	// VRC6 (KONAMI SOUND CHIP)
	else if (address >= VRC6_MIN_BASE)
	{
    	if (mapper == 24 || mapper == 26)
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
}

// Needs review
void __fastcall APU4015Reg()
{
	static int oldkey = 0;
	int key = 0;
	if (apu.square[0].key && apu.square[0].lc.counter)
	{
		key |= APU_CH_PULSE1;
	}
	if (apu.square[1].key && apu.square[1].lc.counter) 
	{
		key |= APU_CH_PULSE2;
	}
	if (apu.triangle.key && apu.triangle.lc.counter && apu.triangle.li.counter)
	{
		key |= APU_CH_TRIANGLE;
	}
	if (apu.noise.key && apu.noise.lc.counter) 
	{
		key |= APU_CH_NOISE;
	}
	if (apu.dpcm.length) 
	{
		key |= APU_CH_DMC;
	}
	
	key = key | APU_STATUS_FRAME_IRQ | apu.dpcm.irq_report;
	if (oldkey != key || apuirq) 
	{
		IPC_REG4015 = key;
		IPC_APUIRQ = apuirq;
		oldkey = key;
		apuirq = 0;
	}
}

static void NESAPUSoundSquareReset(NESAPU_SQUARE *ch)
{
	XMEMSET(ch, 0, sizeof(NESAPU_SQUARE));
		if(getApuCurrentRegion() == PAL)
	{
		ch->cps = GetFixedPointStep((NES_BASECYCLES << 1), 13 * (NESAudioFrequencyGet() << 1), CPS_SHIFT);
	}
	else
	{
		ch->cps = GetFixedPointStep(NES_BASECYCLES, 12 * NESAudioFrequencyGet(), CPS_SHIFT);
	}
}
static void NESAPUSoundTriangleReset(NESAPU_TRIANGLE *ch)
{
	XMEMSET(ch, 0, sizeof(NESAPU_TRIANGLE));
	if(getApuCurrentRegion() == PAL)
	{
		ch->cps = GetFixedPointStep((NES_BASECYCLES << 1), 13 * (NESAudioFrequencyGet() << 1), CPS_SHIFT);
	}
	else
	{
		ch->cps = GetFixedPointStep(NES_BASECYCLES, 12 * NESAudioFrequencyGet(), CPS_SHIFT);
	}
}

static void NESAPUSoundNoiseReset(NESAPU_NOISE *ch)
{
	XMEMSET(ch, 0, sizeof(NESAPU_NOISE));
	ch->cps = GetFixedPointStep(NES_BASECYCLES, 12 * NESAudioFrequencyGet(), CPS_SHIFT);
	ch->rng = 1;
}

static void NESAPUSoundDpcmReset(NESAPU_DPCM *ch)
{
	XMEMSET(ch, 0, sizeof(NESAPU_DPCM));
	if(getApuCurrentRegion() == PAL)
	{
		ch->cps = GetFixedPointStep((NES_BASECYCLES << 1), 13 * (NESAudioFrequencyGet() << 1), CPS_SHIFT);
	}
	else
	{
		ch->cps = GetFixedPointStep(NES_BASECYCLES, 12 * NESAudioFrequencyGet(), CPS_SHIFT);
	}
}

static void __fastcall APUSoundReset(void)
{
	Uint i;
	NESAPUSoundSquareReset(&apu.square[0]);
	NESAPUSoundSquareReset(&apu.square[1]);
	NESAPUSoundTriangleReset(&apu.triangle);
	NESAPUSoundNoiseReset(&apu.noise);
	NESAPUSoundDpcmReset(&apu.dpcm);
	apu.cpf[1] = GetFixedPointStep(NES_BASECYCLES, 12 * 240, CPS_SHIFT);
	apu.cpf[2] = GetFixedPointStep(NES_BASECYCLES, 12 * 240 * 4 / 5, CPS_SHIFT);
	apu.cpf[0] = apu.cpf[1];
	apu.square[1].sw.ch = 1;
	apu.square[0].cpf = &apu.cpf[0];
	apu.square[1].cpf = &apu.cpf[0];
	apu.triangle.cpf = &apu.cpf[0];
	apu.noise.cpf = &apu.cpf[0];
	apu.triangle.li.cpf = apu.cpf[1];

	for (i = 0; i <= 0x17; i++)
	{
		APUSoundWrite(0x4000 + i, (i == 0x10) ? 0x10 : 0x00);
	}
	APUSoundWrite(0x4015, 0x0f);
#if 1
	apu.dpcm.first = 1;
#endif
}

static NES_RESET_HANDLER s_apu_reset_handler[] = {
	{ NES_RESET_SYS_NOMAL, APUSoundReset, 0}, 
	{ 0,                   0, 0}
};

void APUSoundInstall(void)
{
	NESResetHandlerInstall(s_apu_reset_handler);
}