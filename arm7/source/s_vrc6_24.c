#include "s_vrc6.h"

// (:::) VRC6 AUDIO ENGINE (:::) //

static Int32 VRC6SoundSquareRender(VRC6_SQUARE *ch)
{
    if (!ch->spd) return 0;

    ch->cycles -= ch->cps;
    while (ch->cycles < 0)
    {
        ch->cycles += ch->spd;
        ch->adr++;
    }
    ch->adr &= 0xF;

    if (ch->mute || !(ch->regs[vrc6s.p_high] & 0x80)) return 0;

    Uint32 output = ch->regs[0] & 0x0F;
    if (!(ch->regs[0] & 0x80) && (ch->adr < ((ch->regs[0] >> 4) + 1)))
    {
        return 0;
    }
    return output;
}

static Int32 VRC6SoundSawRender(VRC6_SAW *ch)
{
    // If spd is 0 or the channel is disabled (bit 7 register mapped to period high)
    if (!ch->spd || ch->mute || !(ch->regs[vrc6s.p_high] & 0x80)) return 0;

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

static Int32 __fastcall VRC6SoundRender(void)
{
	Int32 accum = 0;
	accum += VRC6SoundSquareRender(&vrc6s.square[0]);
	accum += VRC6SoundSquareRender(&vrc6s.square[1]);
	accum += VRC6SoundSawRender(&vrc6s.saw);
	return accum;
}

int32_t VRC6SoundRender1(void)
{
	return VRC6SoundSquareRender(&vrc6s.square[0]);
}

int32_t VRC6SoundRender2(void)
{
	return VRC6SoundSquareRender(&vrc6s.square[1]);
}

int32_t VRC6SoundRender3(void)
{
	return VRC6SoundSawRender(&vrc6s.saw);
}

static NES_AUDIO_HANDLER s_vrc6_audio_handler[] = {
	{ 1, VRC6SoundRender, }, 
	{ 0, 0, },
};

static void __fastcall VRC6SoundVolume(Uint volume)
{
	volume += 64;
	//vrc6s.mastervolume = (volume << (LOG_BITS - 8)) << 1;
}

static NES_VOLUME_HANDLER s_vrc6_volume_handler[] = {
	{ VRC6SoundVolume, },
	{ 0, }, 
};

void VRC6SoundWrite9000(Uint address, Uint value)
{
    int reg = address & 3;
    VRC6_SQUARE *ch = &vrc6s.square[0];
    ch->regs[reg] = value;

    // If the pulse Low or High regs were written,
    // recalculate SPD immediately
    if (reg == vrc6s.p_high || reg == vrc6s.p_low)
    {
        ch->spd = (((ch->regs[vrc6s.p_high] & 0x0F) << 8) + ch->regs[vrc6s.p_low]) << CPS_SHIFT;
    }
}

void VRC6SoundWriteA000(Uint address, Uint value)
{
    int reg = address & 3;
    VRC6_SQUARE *ch = &vrc6s.square[1];
    ch->regs[reg] = value;

	// If the pulse Low or High regs were written,
    // recalculate SPD immediately
    if (reg == vrc6s.p_high || reg == vrc6s.p_low)
    {
        ch->spd = (((ch->regs[vrc6s.p_high] & 0x0F) << 8) + ch->regs[vrc6s.p_low]) << CPS_SHIFT;
    }
}

void VRC6SoundWriteB000(Uint address, Uint value)
{
    int reg = address & 3;
    VRC6_SAW *ch = &vrc6s.saw;
    ch->regs[reg] = value;

	// If the pulse Low or High regs were written,
    // recalculate SPD immediately
    if (reg == vrc6s.p_high || reg == vrc6s.p_low)
    {
        ch->spd = (((ch->regs[vrc6s.p_high] & 0x0F) << 8) + ch->regs[vrc6s.p_low]) << CPS_SHIFT;
    }
}

// static NES_WRITE_HANDLER s_vrc6_write_handler[] =
// {
// 	{ 0x9000, 0x9002, VRC6SoundWrite9000, },
// 	{ 0xA000, 0xA002, VRC6SoundWriteA000, },
// 	{ 0xB000, 0xB002, VRC6SoundWriteB000, },
// 	{ 0,      0,      0, 				  },
// };

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

// @brief On some boards (Mapper 26), the A0 and A1 lines were switched, so for those, 
// registers will need adjustment ($x001 will become $x002 and vice versa). 
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

static NES_RESET_HANDLER s_vrc6_reset_handler[] = {
	{ NES_RESET_SYS_NOMAL, VRC6SoundReset, }, 
	{ 0,                   0, }, 
};

void VRC6SoundInstall(void)
{
	NESAudioHandlerInstall(s_vrc6_audio_handler);
	NESVolumeHandlerInstall(s_vrc6_volume_handler);
	NESResetHandlerInstall(s_vrc6_reset_handler);
}
