#include "s_vrc6.h"

// (:::) VRC6 AUDIO ENGINE (:::) //
// Based on the VRC6 Audio spec in https://www.nesdev.org/wiki/VRC6_audio

static VRC6SOUND vrc6s;

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
        ch->adr++;
    }
    ch->adr &= 0xF;

    Uint32 output = ch->regs[0] & 0x0F;
    if (!(ch->regs[0] & 0x80) && (ch->adr < ((ch->regs[0] >> 4) + 1)))
    {
        return 0;
    }
    return output;
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
        ch->output += (ch->regs[0] & 0x3F); // 6bit Accum
        if (7 == ++ch->adr) // VRC6 Saw has 7 steps
        {
            ch->adr = 0;
            ch->output = 0;
        }
    }

    return (ch->output >> 3) & 0x1f; // Final Adjustment
}

int32_t VRC6SoundRenderSquare1(void)
{
	return VRC6SoundSquareRender(&vrc6s.square[0]);
}

int32_t VRC6SoundRenderSquare2(void)
{
	return VRC6SoundSquareRender(&vrc6s.square[1]);
}

int32_t VRC6SoundRenderSaw(void)
{
	return VRC6SoundSawRender(&vrc6s.saw);
}

static void __fastcall VRC6SoundVolume(Uint volume)
{
	volume += 64;
}

static NES_VOLUME_HANDLER s_vrc6_volume_handler[] =
{
	{ VRC6SoundVolume, },
	{ 0, }, 
};

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

Uint32 DivFix(Uint32 p1, Uint32 p2, Uint32 fix)
{
	Uint32 ret;
	ret = p1 / p2;
	p1  = p1 % p2;
	//p1 = p1 - p2 * ret;
	while (fix--)
	{
		p1 += p1;
		ret += ret;
		if (p1 >= p2)
		{
			p1 -= p2;
			ret++;
		}
	}
	return ret;
}

void VRC6SoundSquareReset(VRC6_SQUARE *ch)
{
	if(getApuCurrentRegion() == PAL)
	{
		ch->cps = DivFix((NES_BASECYCLES << 1), 13 * (NESAudioFrequencyGet() << 1), CPS_SHIFT);
	}
	else
	{
		ch->cps = DivFix(NES_BASECYCLES, 12 * NESAudioFrequencyGet(), CPS_SHIFT);
	}
}

void __fastcall VRC6SoundSawReset(VRC6_SAW *ch)
{
	if(getApuCurrentRegion() == PAL)
	{
		ch->cps = DivFix((NES_BASECYCLES << 1), 26 * (NESAudioFrequencyGet() << 1), CPS_SHIFT);
	}
	else
	{
		ch->cps = DivFix(NES_BASECYCLES, 24 * NESAudioFrequencyGet(), CPS_SHIFT);
	}
}

static NES_RESET_HANDLER s_vrc6_reset_handler[] =
{
	{ NES_RESET_SYS_NOMAL, VRC6SoundReset, }, 
	{ 0,                   0, }, 
};

/// @brief On some boards (Mapper 26), the A0 and A1 lines were switched, so for those, 
/// registers will need adjustment ($x001 will become $x002 and vice versa). 
static void VRC6SoundSetPulseLineRegs()
{
	if (IPC_MAPPER == 24)
	{
		// 悪魔城伝説 (Akumajou Densetsu, iNES mapper 024)
        vrc6s.p_high = 2;
        vrc6s.p_low = 1;
    }
	else
	{
		// For Madara and Esper Dream 2 and some VRC6 romhacks (iNES mapper 026)
        vrc6s.p_high = 1;
        vrc6s.p_low = 2;
    }
}

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
	NESVolumeHandlerInstall(s_vrc6_volume_handler);
	NESResetHandlerInstall(s_vrc6_reset_handler);
}
