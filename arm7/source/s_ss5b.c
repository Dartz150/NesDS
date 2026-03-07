#include <string.h>
#include "nestypes.h"
#include "audiosys.h"
#include "c_defs.h"
#include "s_ss5b.h"
#include "soundChannel.h"

// (:::) SUNSOFT 5B AUDIO ENGINE (:::) //
// Based on the S5B Audio spec in https://www.nesdev.org/wiki/Sunsoft_5B_audio, code by "DartzSoryu".

typedef struct {
    u16 period;    // 12-bit frequency period
    u8 volume;     // 4-bit volume + 1-bit envelope mode
    u8 mute;
} SS5B_SQUARE;

typedef struct {
    SS5B_SQUARE squares[3];
    u8 mixer;       // Reg $07: --CB Acba (Noise/Tone disable)
    u8 noise_per;   // Reg $06
} SS5B_SOUND;

static SS5B_SOUND ss5bs;

/**
 * @brief Logarithmic Volume Table for AY-3-8910 (1.5dB steps).
 * Maps 4-bit (0-15) to DS Volume (0-127).
 * Values calculated to maintain the characteristic "crunchy" attenuation of the 5B.
 */
static const u8 volTable5B[16] = {
    0, 2, 3, 4, 6, 8, 11, 16, 23, 32, 45, 64, 78, 96, 112, 127
};

void ss5bSoundWrite(Uint address, Uint value)
{
    switch(address) {
        // Tone Periods
        case 0x00: case 0x01: // Ch A
        case 0x02: case 0x03: // Ch B
        case 0x04: case 0x05: // Ch C
        {
            int ch = address >> 1;
            if (address & 1) ss5bs.squares[ch].period = (ss5bs.squares[ch].period & 0x00FF) | ((value & 0x0F) << 8);
            else ss5bs.squares[ch].period = (ss5bs.squares[ch].period & 0x0F00) | value;
            break;
        }
        case 0x06: // Noise Period
            ss5bs.noise_per = value & 0x1F;
            break;
        case 0x07: // Mixer
            ss5bs.mixer = value;
            break;
        case 0x08: case 0x09: case 0x0A: // Volume
            ss5bs.squares[address - 0x08].volume = value & 0x1F; // Bit 4 is the Envelope flag
            break;
        // Envelope regs ($0B-$0D) could be added here for full 5B support, but are never used.
    }
}

static void ss5bSoundSquareUpdateHw(int ch_idx, int ds_chan, int pan, Uint32 nes_apu_clock)
{
    SS5B_SQUARE *ch = &ss5bs.squares[ch_idx];
    
    // The Mixer register ($07) bit 0 enables tone, 1 disables it.
    bool tone_disabled = (ss5bs.mixer >> ch_idx) & 0x01;
    u8 volume = ch->volume & 0x0F;
    bool env_enabled = (ch->volume >> 4) & 0x01;

    // Volume gate, Tone must be enabled in mixer AND have volume.
    // (Note: For Gimmick! we ignore Envelope for now as it's rarely used for the main squares)
    if (tone_disabled || volume == 0 || ch->mute)
    {
        if (snd_isChannelPlaying(ds_chan)) snd_stopChannel(ds_chan);
        return;
    }

    // Freq = Clock / (32 * Period). 
    // We use shift = 4 to account for the /32 factor (The NES APU uses /16).
    // If period is 0 or 1, it's treated as 1.
    u16 period = (ch->period < 2) ? 1 : ch->period;
    u16 ds_timer = nesToDsTimer(period, nes_apu_clock, 4, 0);

    // Filter out ultrasonic frequencies that crash the DS timer
    if (ds_timer < 0x0010) {
        if (snd_isChannelPlaying(ds_chan)) snd_stopChannel(ds_chan);
        return;
    }

    // Apply the logarithmic volume LUT
    u32 ds_vol = (volTable5B[volume]) >> 1;

    if (!snd_isChannelPlaying(ds_chan))
    {
        REG_SOUNDxTMR(ds_chan) = ds_timer;
        REG_SOUNDxCNT(ds_chan) = SOUNDCNT_ENABLED | SOUNDCNT_FORMAT_PSG | 
                                 SOUNDCNT_MODE_LOOP | SOUNDCNT_DUTY_50_0 | // Fixed 50% duty
                                 SOUNDCNT_PAN(pan) | SOUNDCNT_VOLUME(ds_vol);
    }
    else
    {
        REG_SOUNDxTMR(ds_chan) = ds_timer;
        u32 current_cnt = REG_SOUNDxCNT(ds_chan);
        u32 new_cnt = (current_cnt & ~0x7F) | ds_vol;
        if (current_cnt != new_cnt) REG_SOUNDxCNT(ds_chan) = new_cnt;
    }
}

void ss5bSoundHwUpdate(Uint32 nes_apu_clock, Uint32 ds_sound_freq)
{
    if (!has_ss5b) return;

    // Panning
    u32 s5b_pu1_pan, s5b_pu2_pan, s5b_pu3_pan;
    if (apu_cfg.stereo)
    {
        s5b_pu1_pan = 48;
        s5b_pu2_pan = 80;
        s5b_pu3_pan = 64;
    }
    else
    {
        s5b_pu1_pan = 64;
        s5b_pu2_pan = 64;
        s5b_pu3_pan = 64;
    }

    // The Sunsoft 5B is loud, we spread the panning slightly.
    (apu_cfg.ss5b_p1)
        ? snd_stopChannel(DS_SS5B_SQ1_CH)
        : ss5bSoundSquareUpdateHw(0, DS_SS5B_SQ1_CH, s5b_pu1_pan, nes_apu_clock);
    
    (apu_cfg.ss5b_p2)
        ? snd_stopChannel(DS_SS5B_SQ2_CH)
        : ss5bSoundSquareUpdateHw(1, DS_SS5B_SQ2_CH, s5b_pu2_pan, nes_apu_clock);

    (apu_cfg.ss5b_p3)
        ? snd_stopChannel(DS_SS5B_SQ3_CH)
        : ss5bSoundSquareUpdateHw(2, DS_SS5B_SQ3_CH, s5b_pu3_pan, nes_apu_clock);
}

void ss5bSoundHwStop()
{
    snd_stopChannel(DS_SS5B_SQ1_CH);
    snd_stopChannel(DS_SS5B_SQ2_CH);
    snd_stopChannel(DS_SS5B_SQ3_CH);
}

void ss5bSoundInit()
{
    memset(&ss5bs, 0, sizeof(SS5B_SOUND));
    ss5bs.mixer = 0xFF; // All pulses disabled by default
    
    snd_stopChannel(DS_SS5B_SQ1_CH);
    snd_stopChannel(DS_SS5B_SQ2_CH);
    snd_stopChannel(DS_SS5B_SQ3_CH);
}
