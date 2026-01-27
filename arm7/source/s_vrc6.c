#include <string.h>
#include "nestypes.h"
#include "audiosys.h"
#include "handler.h"
#include "c_defs.h"
#include "s_vrc6.h"

// (:::) VRC6 AUDIO ENGINE (:::) //
// Based on the VRC6 Audio spec in https://www.nesdev.org/wiki/VRC6_audio and previous code by "huiminghao".

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

#define VRC6_MIX_FACTOR 380 //  23,180 (NES pulse weight) / (15 + 15 + 31 = 61) ≈ 380
static VRC6SOUND vrc6s;

/// @brief On some boards (Mapper 26), the A0 and A1 lines were switched, so for those, 
/// registers will need adjustment ($x001 will become $x002 and vice versa). 
static void VRC6SoundSetPulseLineRegs()
{
	int is_vrc6_24 = (IPC_MAPPER == 24);
    // 悪魔城伝説 (Akumajou Densetsu, iNES mapper 024)
    vrc6s.p_high = is_vrc6_24 ? 2 : 1;

    // For Madara,  Esper Dream 2 and some VRC6 romhacks (iNES mapper 026)
    vrc6s.p_low  = is_vrc6_24 ? 1 : 2;
}

static Int32 VRC6SoundSquareRender(VRC6_SQUARE *ch)
{
	// When the channel is disabled by clearing the E bit (0x80), output is forced to 0, 
	// and the duty cycle is immediately reset and halted.
	if (!ch->spd || ch->mute || !(ch->regs[vrc6s.p_high] & 0x80)) 
    {
        return 0;
    }

    ch->cycles -= ch->cps;
    while (ch->cycles < 0)
    {
        ch->cycles += ch->spd;
        // Spec: this counts from 15 to 0
        if (ch->adr == 0) ch->adr = 15;
        else ch->adr--;
    }
    
    Uint8 volume = ch->regs[0] & 0x0F;
    Uint8 duty   = (ch->regs[0] >> 4) & 0x07;
    bool mode    = (ch->regs[0] & 0x80); // Bit 7 from the first reg is Mode

    // If Mode is 1, ignore duty and return volume.
    if (mode) return volume;

    // If the current step is <= Duty, return volume, otherwise 0.
    // This generates a inverted pulse, per the nesDev spec.
    return (ch->adr <= duty) ? volume : 0;
}

static Int32 VRC6SoundSawRender(VRC6_SAW *ch)
{
    // When the channel is disabled by clearing the E bit (0x80), output is forced to 0, 
	// and the duty cycle is immediately reset and halted.
    if (!ch->spd || ch->mute || !(ch->regs[vrc6s.p_high] & 0x80))
    {
        return 0;
    }

    ch->cycles -= ch->cps;
    while (ch->cycles < 0)
    {
        ch->cycles += ch->spd;
        // Increment phase (0..13)
        ch->adr++;
        // Accumulator increments only on even phases
        if ((ch->adr & 1) == 0)
        {
            if (ch->adr < 14) 
            {
                ch->output += (ch->regs[0] & 0x3F); // Accum Rate
            }
        }

        // Reset after 14 phases
        if (ch->adr >= 14)
        {
            ch->adr = 0;
            ch->output = 0;
        }
    }

    // Return 5 higher bits from the accum (0-31)
    return (ch->output >> 3) & 0x1F;
}

// VRC6 Mixer. NesDev: "Lineal 6 bit sum (max 61: 15 + 15 + 31)"
int32_t VRC6SoundRender() 
{
    return (VRC6SoundSquareRender(&vrc6s.square[0]) + 
            VRC6SoundSquareRender(&vrc6s.square[1]) + 
            VRC6SoundSawRender(&vrc6s.saw)) * VRC6_MIX_FACTOR;
}

static void VRC6SoundWriteSquare(VRC6_SQUARE *ch, Uint address, Uint value)
{
    int reg = address & 3;
    
    if (reg == vrc6s.p_high)
    {
        // Reset phase if it changes from 1 to 0
        if ((ch->regs[reg] & 0x80) && !(value & 0x80)) ch->adr = 0;
    }

    ch->regs[reg] = value;

	// If the pulse Low or High regs were written,
    // recalculate SPD immediately
    if (reg == vrc6s.p_high || reg == vrc6s.p_low)
    {
        ch->spd = (((ch->regs[vrc6s.p_high] & 0x0F) << 8) + ch->regs[vrc6s.p_low]) << CPS_SHIFT;
    }
}

void VRC6SoundWrite9000(Uint address, Uint value)
{
	VRC6SoundWriteSquare(&vrc6s.square[0], address, value);
}

void VRC6SoundWriteA000(Uint address, Uint value) 
{
	VRC6SoundWriteSquare(&vrc6s.square[1], address, value);
}

void VRC6SoundWriteB000(Uint address, Uint value)
{
    int reg = address & 3;
    VRC6_SAW *ch = &vrc6s.saw;

    if (reg == vrc6s.p_high)
	{
		// If bit 7 (Enable) changes from 1 to 0 (Disable)
        if ((ch->regs[reg] & 0x80) && !(value & 0x80))
		{
            ch->adr = 0; // Immediate phase reset
            ch->output = 0; // Saw also resets its accum
        }
    }

    ch->regs[reg] = value;

	// If the pulse Low or High regs were written,
    // recalculate SPD immediately
    if (reg == vrc6s.p_high || reg == vrc6s.p_low)
    {
        ch->spd = (((ch->regs[vrc6s.p_high] & 0x0F) << 8) + ch->regs[vrc6s.p_low]) << CPS_SHIFT;
    }
}

void VRC6SoundSquareReset(VRC6_SQUARE *ch)
{
	int apu_region = (getApuCurrentRegion() == PAL) ? NES_CPU_PAL : NES_CPU_NTSC;
	ch->cps = GetFixedPointStep(apu_region, NESAudioFrequencyGet(), CPS_SHIFT);
}

void __fastcall VRC6SoundSawReset(VRC6_SAW *ch)
{
	int apu_region = (getApuCurrentRegion() == PAL) ? NES_CPU_PAL : NES_CPU_NTSC;
	ch->cps = GetFixedPointStep(apu_region, NESAudioFrequencyGet(), CPS_SHIFT);
}

static NES_RESET_HANDLER s_vrc6_reset_handler[] =
{
	{ NES_RESET_SYS_NOMAL, VRC6SoundReset, }, 
	{ 0,                   0, }, 
};

void __fastcall VRC6SoundReset(void)
{
	XMEMSET(&vrc6s, 0, sizeof(VRC6SOUND));
    VRC6SoundSetPulseLineRegs();
	VRC6SoundSquareReset(&vrc6s.square[0]);
	VRC6SoundSquareReset(&vrc6s.square[1]);
	VRC6SoundSawReset(&vrc6s.saw);
}

void VRC6SoundInstall(void)
{
	NESResetHandlerInstall(s_vrc6_reset_handler);
}
