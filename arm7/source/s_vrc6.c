#include <string.h>
#include "nestypes.h"
#include "audiosys.h"
#include "handler.h"
#include "c_defs.h"
#include "s_vrc6.h"
#include "soundChannel.h"

// (:::) VRC6 AUDIO ENGINE (:::) //
// Based on the VRC6 Audio spec in https://www.nesdev.org/wiki/VRC6_audio and previous code by "huiminghao".

// Pulse channels must be PSG capable channels (8 - 15), channel 0/1 reserved for PCM16 buffers
#define PSG_VRC_SAW_CH          2
#define PSG_VRC_SQUARE_1_CH     DS_PSG_CH9
#define PSG_VRC_SQUARE_2_CH     DS_PSG_CH10

// Wave tables oversampling factor, greater values increase the wave quality, 2 ~ 8 are recommended
#define VRC6_OVERSAMPLE         4

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

static VRC6SOUND vrc6s;

// Saw/Pulse wave tables with duty cycle modes unsupported by the DS PSG mode
static s8 sVrc6SquareHQ[8][16 * VRC6_OVERSAMPLE];
static s8 sVrc6SawHQ[14 * VRC6_OVERSAMPLE];
static s8 sVrc6FullDutyHQ[4 * VRC6_OVERSAMPLE];

/// @brief On some boards (Mapper 26), the A0 and A1 lines were switched, so for those, 
/// registers will need adjustment ($x001 will become $x002 and vice versa). 
static void vrc6SoundSetPulseLineRegs()
{
	int is_vrc6_24 = (IPC_MAPPER == 24);
    // 悪魔城伝説 (Akumajou Densetsu, iNES mapper 024)
    vrc6s.p_high = is_vrc6_24 ? 2 : 1;

    // For Madara,  Esper Dream 2 and some VRC6 romhacks (iNES mapper 026)
    vrc6s.p_low  = is_vrc6_24 ? 1 : 2;
}

// Generate wavetables for the duty cycle modes missing in the PSG mode on the DS
static void generateVrc6SquareHQ()
{
    for (int duty = 0; duty < 8; duty++)
    {
        int high_steps = duty + 1; // 1/16 → 8/16
        int total_steps = 16;
        int out_index = 0;

        for (int step = 0; step < total_steps; step++)
        {
            s8 value = (step < high_steps) ? 127 : -127;
            for (int o = 0; o < VRC6_OVERSAMPLE; o++)
            {
                sVrc6SquareHQ[duty][out_index++] = value;
            }
        }
    }

    // 100% duty
    for (int i = 0; i < 4 * VRC6_OVERSAMPLE; i++)
    {
        sVrc6FullDutyHQ[i] = 127;
    }
}

// Generate Saw wavetable
static void generateVrc6SawHQ()
{
    int out_index = 0;
    for (int step = 0; step < 14; step++)
    {
        s8 value = -64 + ((step * 128) / 14);

        for (int o = 0; o < VRC6_OVERSAMPLE; o++)
        {
            sVrc6SawHQ[out_index++] = value;
        }
    }
}

static void vrc6SoundSquareUpdateHw(VRC6_SQUARE *ch, int ds_chan, int pan)
{
    // When the channel is disabled by clearing the E bit (0x80), output is forced to 0, 
	// and the duty cycle is immediately reset and halted.
    u32 vrc_wl = ((ch->regs[vrc6s.p_high] & 0x0F) << 8) | ch->regs[vrc6s.p_low];
    bool is_enabled = (ch->regs[vrc6s.p_high] & 0x80);
    u8 vrc_duty = (ch->regs[0] >> 4) & 0x07;
    bool duty_100 = (ch->regs[0] & 0x80); // Bit 7 from the first reg is duty mode

    if (vrc_wl < 8 || !is_enabled || ch->mute)
    {
        if (snd_isChannelPlaying(ds_chan)) snd_stopChannel(ds_chan);
        return;
    }

    // Volume and Timers
    u8 volume = ch->regs[0] & 0x0F;
    u16 ds_timer;
    u32 ds_vol = volume << 1;

    // The DS PSG sound channel mode doesn't support all the duty modes required
    // by the VRC6 pulses, so we need to simulate the missing duty modes.
    // Even duty values use the PSG mode, odds use the PCM8 mode as a wave oscillator.
    const s8* pcm_duty = NULL;
    u32 psg_duty = 0;
    bool use_pcm8 = false;

    if (duty_100)
    {
        use_pcm8 = true;
        pcm_duty = sVrc6FullDutyHQ;
    }
    else
    {
        switch (vrc_duty)
        {
            case 0: use_pcm8 = true; pcm_duty = sVrc6SquareHQ[vrc_duty]; break; // 1/16
            case 1: psg_duty = SOUNDCNT_DUTY_12_5; break;              // 2/16
            case 2: use_pcm8 = true; pcm_duty = sVrc6SquareHQ[vrc_duty]; break; // 3/16
            case 3: psg_duty = SOUNDCNT_DUTY_25_0; break;              // 4/16
            case 4: use_pcm8 = true; pcm_duty = sVrc6SquareHQ[vrc_duty]; break; // 5/16
            case 5: psg_duty = SOUNDCNT_DUTY_37_5; break;              // 6/16
            case 6: use_pcm8 = true; pcm_duty = sVrc6SquareHQ[vrc_duty]; break; // 7/16
            case 7: psg_duty = SOUNDCNT_DUTY_50_0; break;              // 8/16
        }
    }

     // PCM8 mode runs like an oscillator, using wavetables as the waveform source.
    if (use_pcm8)
    {
        // PCM8 with 16 a bit table, shift 1 so 1 byte = 1 WL tick
        ds_timer = nesToDsTimer(vrc_wl, NES_CPU_NTSC, 1, 1);

        // Apply the oversampling factor if better wave qualities are used.
        ds_timer = (DS_SOUND_FREQUENCY << 1) - (((DS_SOUND_FREQUENCY << 1) - ds_timer) / VRC6_OVERSAMPLE);

        if (!snd_isChannelPlaying(ds_chan) || (REG_SOUNDxCNT(ds_chan) & SOUNDCNT_FORMAT_PSG))
        {
            snd_stopChannel(ds_chan);
            REG_SOUNDxSAD(ds_chan) = (u32)pcm_duty;
            REG_SOUNDxLEN(ds_chan) = (16 * VRC6_OVERSAMPLE) >> 2;
            REG_SOUNDxPNT(ds_chan) = 0;
            REG_SOUNDxTMR(ds_chan) = ds_timer;
            REG_SOUNDxCNT(ds_chan) = SOUNDCNT_ENABLED | SOUNDCNT_FORMAT_PCM8 | 
                                     SOUNDCNT_MODE_LOOP | SOUNDCNT_PAN(pan) | SOUNDCNT_VOLUME(ds_vol);
        }
        else
        {
            REG_SOUNDxTMR(ds_chan) = ds_timer;
            REG_SOUNDxCNT(ds_chan) = (REG_SOUNDxCNT(ds_chan) & ~0x7F) | SOUNDCNT_VOLUME(ds_vol);
            // Each time the duty value changes, we must update the duty source
            if (REG_SOUNDxSAD(ds_chan) != (u32)pcm_duty)
            {
                REG_SOUNDxSAD(ds_chan) = (u32)pcm_duty;
            }
        }
    }
    // PSG Mode runs the square waves as normal, using its internal duty cycle modes.
    else
    {
        // The DS PSG hardware already does the /16 div internally, shift 2
        // VRC6 titles are always NTSC
        ds_timer = nesToDsTimer(vrc_wl, NES_CPU_NTSC, 2, 1);

        if (!snd_isChannelPlaying(ds_chan) || !(REG_SOUNDxCNT(ds_chan) & SOUNDCNT_FORMAT_PSG))
        {
            snd_stopChannel(ds_chan);
            REG_SOUNDxTMR(ds_chan) = ds_timer;
            REG_SOUNDxCNT(ds_chan) = SOUNDCNT_ENABLED | SOUNDCNT_FORMAT_PSG | 
                                     SOUNDCNT_MODE_LOOP | psg_duty | 
                                     SOUNDCNT_PAN(pan) | SOUNDCNT_VOLUME(ds_vol);
        }
        else
        {
            REG_SOUNDxTMR(ds_chan) = ds_timer;
            u32 current_cnt = REG_SOUNDxCNT(ds_chan);
            u32 new_cnt = (current_cnt & ~(0x7F | SOUNDCNT_DUTY_MASK)) | ds_vol | psg_duty;
            if (current_cnt != new_cnt)
            {
                REG_SOUNDxCNT(ds_chan) = new_cnt;
            }
        }
    }
}

// We also use a PCM8 channel as a wave oscillator for the Saw channel
static void vrc6SoundSawUpdateHw(VRC6_SAW *ch, int ds_chan, int pan)
{
    // When the channel is disabled by clearing the E bit (0x80), output is forced to 0, 
	// and the duty cycle is immediately reset and halted.
    u32 vrc_wl = ((ch->regs[vrc6s.p_high] & 0x0F) << 8) | ch->regs[vrc6s.p_low];
    bool is_enabled = (ch->regs[vrc6s.p_high] & 0x80);

    if (vrc_wl < 8 || !is_enabled || ch->mute)
    {
        if (snd_isChannelPlaying(ds_chan)) snd_stopChannel(ds_chan);
        return;
    }

    // 1 DS sample = 1 VRC6 base cycle (WL+1).
    // VRC6 titles are always NTSC
    u16 ds_timer = nesToDsTimer(vrc_wl, NES_CPU_NTSC, 1, 1);

    // Apply the oversampling factor if better wave qualities are used.
    ds_timer = (DS_SOUND_FREQUENCY << 1) - (((DS_SOUND_FREQUENCY << 1) - ds_timer) / VRC6_OVERSAMPLE);

    // The Saw is an accumulator that increments only on even phases,
    // we simulate with a pre-accumulated 28 byte table.
    u8 ds_vol = (ch->regs[0] & 0x3F) * 3; // Volume: 6 bits to 7 bits (0..63 -> 0..126)

    if (!snd_isChannelPlaying(ds_chan))
    {
        REG_SOUNDxSAD(ds_chan) = (u32)sVrc6SawHQ;
        REG_SOUNDxLEN(ds_chan) =
            (14 * VRC6_OVERSAMPLE) >> 2; // 14-step waveform cycle matching VRC6 internal accumulator behavior
        REG_SOUNDxPNT(ds_chan) = 0;
        REG_SOUNDxTMR(ds_chan) = ds_timer;
        REG_SOUNDxCNT(ds_chan) = SOUNDCNT_ENABLED | SOUNDCNT_FORMAT_PCM8 | 
                                 SOUNDCNT_MODE_LOOP | SOUNDCNT_PAN(pan) | SOUNDCNT_VOLUME(ds_vol);
    }
    else
    {
        REG_SOUNDxTMR(ds_chan) = ds_timer;
        u32 current_cnt = REG_SOUNDxCNT(ds_chan);
        // We only update the volume to avoid sound clicks, keep the sound format
        u32 new_cnt = (current_cnt & ~0x7F) | SOUNDCNT_VOLUME(ds_vol);
        if (current_cnt != new_cnt)
        {
            REG_SOUNDxCNT(ds_chan) = new_cnt;
        }
    }
}

void VRC6SoundHwUpdate()
{
    if (!has_vrc6) return;

    // Panning
    u32 v_pu1_pan, v_pu2_pan;
    if (apu_cfg.stereo)
    {
        v_pu1_pan = 72;
        v_pu2_pan = 56;
    }
    else
    {
        v_pu1_pan = 64;
        v_pu2_pan = 64;
    }

    (apu_cfg.vrc_p1)
        ? snd_stopChannel(PSG_VRC_SQUARE_1_CH)
        : vrc6SoundSquareUpdateHw(&vrc6s.square[0], PSG_VRC_SQUARE_1_CH, v_pu1_pan);

    (apu_cfg.vrc_p2)
        ? snd_stopChannel(PSG_VRC_SQUARE_2_CH)
        : vrc6SoundSquareUpdateHw(&vrc6s.square[1], PSG_VRC_SQUARE_2_CH, v_pu2_pan);

    (apu_cfg.vrc_saw)
        ? snd_stopChannel(PSG_VRC_SAW_CH)
        : vrc6SoundSawUpdateHw(&vrc6s.saw, PSG_VRC_SAW_CH, 64);
}

void VRC6SoundHwStop()
{
    snd_stopChannel(PSG_VRC_SQUARE_1_CH);
    snd_stopChannel(PSG_VRC_SQUARE_2_CH);
    snd_stopChannel(PSG_VRC_SAW_CH);
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

static NES_RESET_HANDLER s_vrc6_reset_handler[] =
{
	{ NES_RESET_SYS_NOMAL, VRC6SoundReset, }, 
	{ 0,                   0, }, 
};

void __fastcall VRC6SoundReset(void)
{
	XMEMSET(&vrc6s, 0, sizeof(VRC6SOUND));
    vrc6SoundSetPulseLineRegs();
    generateVrc6SquareHQ();
    generateVrc6SawHQ();
}

void VRC6SoundInstall(void)
{
	NESResetHandlerInstall(s_vrc6_reset_handler);
}
