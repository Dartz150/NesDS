#include <string.h>
#include "nestypes.h"
#include "audiosys.h"
#include "c_defs.h"
#include "s_mmc5.h"
#include "soundChannel.h"

// (:::) MMC5 AUDIO ENGINE (:::) //
// Based on the MMC5 Audio spec in https://www.nesdev.org/wiki/MMC5_audio, code by "DartzSoryu".

typedef struct
{
    Uint8 regs[4];
    Uint8 mute;
} MMC5_SQUARE;

typedef struct
{
    Uint8 pcm_val;
    Uint8 mode;     // $5010 bit 0
    Uint8 irq_en;   // $5010 bit 7
    Uint8 mute;
    Uint32 rpcm_idx;

} MMC5_PCM;

typedef struct
{
    MMC5_SQUARE square[2];
    MMC5_PCM pcm;
    Uint8 status;   // $5015
} MMC5SOUND;

static MMC5SOUND mmc5s;

// Constant buffer to emulate the DAC through a DC offset.
// A 127 constant value allows us to use the DS volume register as the MMC5 DAC.
static const s8 sMMC5PcmDAC[16] =
{
    127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127, 127, 127
};

/// @brief Plays PSG Square Pulse waves for the MMC5 sound expansion chip, using the DS native hardware.
/// @param ch      NES sound channel data pointer
/// @param ds_chan Sets the channel the DS is going to use. 0-15
/// @param pan     Sets the DS sound channel panning.
///                64:  Center 
///                0:   Left
///                127: Right.
static void mmc5SoundSquareUpdateHw(int ch_idx, int ds_chan, int pan, Uint32 nes_apu_clock)
{
    MMC5_SQUARE *ch = &mmc5s.square[ch_idx];
    
    // Same WL for both pulse channels
    u32 wl = ((ch->regs[3] & 0x07) << 8) | ch->regs[2];
    u8 volume = ch->regs[0] & 0x0F;
    u8 duty_idx = (ch->regs[0] >> 6) & 0x03;
    
    // $5015 status flag enables/disables the channel
    bool enabled = (mmc5s.status >> ch_idx) & 0x01;

    // Volume gate
    if (!enabled || ch->mute || volume == 0)
    {
        if (snd_isChannelPlaying(ds_chan)) snd_stopChannel(ds_chan);
        return;
    }

    u16 ds_timer = nesToDsTimer(wl, nes_apu_clock, 2, 1);
    
    // If (wl < 8), the MMC5 keeps emiting sound, inaudible in a real NES.
    // Interistingly, this causes timer overflows to the DS audio hardware,
    // in the form of high pitched and loud sounds, so we need to filter this manually.
    // Weirdly enough, No$GBA can't reproduce this issue.
    if (wl < 8 || ds_timer < 0x0010)
    {
        if (wl < 4)
        { 
            if (snd_isChannelPlaying(ds_chan)) snd_stopChannel(ds_chan);
            return;
        }
    }

    // Volume target x2 is sufficient, although the spec states the MMC5 pulse waves
    // are louder compared to the APU pulses.
    u32 ds_vol = volume << 1; 
    u32 psg_duty;

    // The Duty values almost matches the register values from the DS, but 2 and 3 are different,
    // so we still need to map them using a switch.
    switch (duty_idx)
    {
        case 0: psg_duty = SOUNDCNT_DUTY_12_5; break;
        case 1: psg_duty = SOUNDCNT_DUTY_25_0; break;
        case 2: psg_duty = SOUNDCNT_DUTY_50_0; break;
        case 3: psg_duty = SOUNDCNT_DUTY_75_0; break; // Emualte the MMC5 phase inversion by using 75% instead of 25%
        default: psg_duty = SOUNDCNT_DUTY_50_0;
    }

    if (!snd_isChannelPlaying(ds_chan))
    {
        REG_SOUNDxTMR(ds_chan) = ds_timer;
        REG_SOUNDxCNT(ds_chan) = SOUNDCNT_ENABLED | SOUNDCNT_FORMAT_PSG | 
                                 SOUNDCNT_MODE_LOOP | psg_duty | 
                                 SOUNDCNT_PAN(pan) | SOUNDCNT_VOLUME(ds_vol);
    }
    else
    {
        // Only update if the volume changes
        REG_SOUNDxTMR(ds_chan) = ds_timer;
        u32 current_cnt = REG_SOUNDxCNT(ds_chan);
        u32 new_cnt = (current_cnt & ~(0x7F | SOUNDCNT_DUTY_MASK)) | ds_vol | psg_duty;
        if (current_cnt != new_cnt) REG_SOUNDxCNT(ds_chan) = new_cnt;
    }
}

void mmc5VblankSync()
{
    mmc5s.pcm.rpcm_idx = 0;
}

/**
 * @brief Frame synchronized NES DMC ($5011) DAC write reconstruction.
 *
 * According to the spec, this works in the same way as the $4011 RAW PCM writes,
 * so we try to use the same method. Currently untested.
 *
 * ARM7 generates audio at the defined [DS_SOUND_FREQUENCY] rate and reconstructs the timing
 * by mapping each produced audio sample to a corresponding NES scanline
 * within the current frame. When a valid timestamped write is detected,
 * the MMC5 DAC output should be updated at the correct point in the audio timeline.
 */
static void mmc5_ReplayPcmWrites(int sample_in_buffer)
{
    unsigned char *pcm_buffer = (unsigned char *)IPC_PCMDATA;
    
    // Map the current DS sample to the NES scanline
    int scanline = (sample_in_buffer * 262) / SAMPLES_PER_DS_FRAME;

    if (scanline >= 262) scanline = 261;

    // If there's a value in this scanliune, update the MMC5 DAC
    // This always assumes we're clearing the buffer at Vblank (mmc5VblankSync();)
    if (pcm_buffer[scanline] != 0)
    {
        mmc5s.pcm.pcm_val = pcm_buffer[scanline];
        pcm_buffer[scanline] = 0; // Avoid processing it twice
    }
}

/// @brief         Plays PCM samples written on the $5011 reg for the MMC5 sound expansion chip.
/// @param ch      NES sound channel data pointer
/// @param ds_chan Sets the channel the DS is going to use. 0-15
/// @param pan     Sets the DS sound channel panning.
///                64:  Center 
///                0:   Left
///                127: Right.
/// @note  This doesn't work properly yet. Only "Shin 4 Nin Uchi Mahjong - Yakuman Tengoku" uses this.
static void mmc5SoundPcmUpdateHw(MMC5_PCM *ch, int ds_chan, int pan)
{
    // Sync with the scanline history before the update
    mmc5_ReplayPcmWrites(ch->rpcm_idx++);

    if (ch->mute)
    {
        if (snd_isChannelPlaying(ds_chan)) snd_stopChannel(ds_chan);
        return;
    }

    // Inverted polarity: If the NES volume increases, should we decrease it then?
    // For the DS, mapping the volume should be sufficient
    // The MMC5 is already 8 bits, we use the direct values (0-255) scaled to the DS range (0-127)
    u8 ds_vol = ch->pcm_val >> 1;

    if (!snd_isChannelPlaying(ds_chan))
    {
        REG_SOUNDxSAD(ds_chan) = (u32)sMMC5PcmDAC;
        REG_SOUNDxLEN(ds_chan) = 16 >> 2;
        REG_SOUNDxPNT(ds_chan) = 0;
        REG_SOUNDxTMR(ds_chan) = TIMER_NFREQ;
        REG_SOUNDxCNT(ds_chan) = SOUNDCNT_ENABLED | SOUNDCNT_FORMAT_PCM8 | 
                                 SOUNDCNT_MODE_LOOP | SOUNDCNT_PAN(pan) | SOUNDCNT_VOLUME(ds_vol);
    }
    else
    {
        u32 current_cnt = REG_SOUNDxCNT(ds_chan);
        // Only update if the volume changes
        if ((current_cnt & 0x7F) != ds_vol)
        {
            REG_SOUNDxCNT(ds_chan) = (current_cnt & ~0x7F) | SOUNDCNT_VOLUME(ds_vol);
        }
    }
}

void mmc5SoundHwUpdate(Uint32 nes_apu_clock, Uint32 ds_sound_freq)
{
    if (!has_mmc5) return;

    mmc5SoundSquareUpdateHw(0, DS_MMC5_SQUARE_1_CH, 72, nes_apu_clock);
    mmc5SoundSquareUpdateHw(1, DS_MMC5_SQUARE_2_CH, 56, nes_apu_clock);
    mmc5SoundPcmUpdateHw(&mmc5s.pcm, DS_MMC5_PCM_CH, 64);
}

void mmc5SoundWrite(Uint address, Uint value)
{
    // Square Pulse Wave 1
    if (address >= 0x5000 && address <= 0x5003)
    {
        mmc5s.square[0].regs[address & 3] = value;
    }
    // Square Pulse Wave 2
    else if (address >= 0x5004 && address <= 0x5007)
    {
        mmc5s.square[1].regs[address & 3] = value;
    }
    // PCM regs
    else if (address == 0x5010)
    {
        mmc5s.pcm.mode = value & 0x01;
        mmc5s.pcm.irq_en = value & 0x80;
    }
    else if (address == 0x5011)
    {
        if (mmc5s.pcm.mode == 0) // Write mode
        {
            // Spec: writing 0 does nothing to the output, but fires the IRQ
            if (value != 0)
            {
                mmc5s.pcm.pcm_val = value;
                // TODO: Handle $5011 irqs
            }
        }
    }
    // Mute flag
    else if (address == 0x5015)
    {
        mmc5s.status = value;
    }
}

void mmc5SoundHwStop()
{
    snd_stopChannel(DS_MMC5_SQUARE_1_CH);
    snd_stopChannel(DS_MMC5_SQUARE_2_CH);
    snd_stopChannel(DS_MMC5_PCM_CH);
}

void mmc5SoundInit()
{
    memset(&mmc5s, 0, sizeof(MMC5SOUND));

    // Start with the channels silenced
    snd_stopChannel(DS_MMC5_SQUARE_1_CH);
    snd_stopChannel(DS_MMC5_SQUARE_2_CH);
    snd_stopChannel(DS_MMC5_PCM_CH);
    
    // Clean control registers
    REG_SOUNDxCNT(DS_MMC5_SQUARE_1_CH) = 0;
    REG_SOUNDxCNT(DS_MMC5_SQUARE_2_CH) = 0;
}
