#include <string.h>
#include "nestypes.h"
#include "audiosys.h"
#include "s_n163.h"
#include "soundChannel.h"

// (:::) NAMCOT 163 AUDIO ENGINE (:::) //
// Based on the N163 Audio spec in https://www.nesdev.org/wiki/Namco_163_audio, code by "DartzSoryu".

typedef struct
{
    u8 ram[128];
    u8 dirty_channels; // Bitmask: bit 0 = ch1, bit 7 = ch8
    u8 phase_dirty;
    u8 num_ch_old;     // Detect changes in the $7F reg
    u8 pan;
} N163_STATE;

static N163_STATE n163s;
static u8 n163_wave_shadow[256]; // RAM converted to PCM8

// One DS channel for each n163 channel, so (2 L/R Ch) + (6 APU Ch) + (8 n163 Ch) = 16/16 DS Channels,
// which sadly means no extra sound expansions with this mapper are supported.
static const u8 n163_ds_channels[8] =
{
    DS_N163_CH_1, DS_N163_CH_2, DS_N163_CH_3, DS_N163_CH_4, 
    DS_N163_CH_5, DS_N163_CH_6, DS_N163_CH_7, DS_N163_CH_8
};

// Panning table: Subtle distribution from the center to the sides
// Channels:   1,  2,  3,  4,  5,  6,  7,  8
// N163_CH:    0,  1,  2,  3,  4,  5,  6,  7
static const u8 n163_pan_table[8] =
{
    54, 74, 44, 84, 60, 68, 50, 78
};

/// @brief      Fetches N163 Wave Ram nibbles in a shadow buffer and converts them to PCM8
/// @param addr Wave nibble address
/// @param val  Wave nibble value
static void n163UpdateShadowWaveRAM(u8 addr, u8 val)
{
    // When the ARM9 writes a nibble, convert to 8 bit immediately
    // N163 (0~15) -> DS PCM8 (-128~127)
    n163_wave_shadow[addr * 2]   = ((val & 0x0F) - 8) << 4;
    n163_wave_shadow[addr * 2 + 1] = ((val >> 4) - 8) << 4;
}

/// @brief        Syncs the DS hardware if a phase change occurs while playing n163 audio. 
/// @param ch_idx DS Hardware Sound Channel channel index.
/// TODO:         Test this behavior properly, retail games never do this?
static void n163SyncPhase(int ch_idx)
{
    u8 ds_ch = n163_ds_channels[ch_idx];
    
    // Don't try to reset the phase on dormant channels
    if (!snd_isChannelPlaying(ds_ch))
    {
        return;
    }

    u8 *regs = &n163s.ram[0x40 + (ch_idx * 8)];
    
    // High phase ($7D) tells the current displacement of the  Wave RAM
    u8 phase_high = regs[5]; 
    u8 wave_adr   = regs[6]; // Wave start point in nibbles

    // Calculate Shadow RAM PCM8 pointer
    // Sum the phase to the wave start point
    u32 new_sad = (u32)&n163_wave_shadow[wave_adr + phase_high];

    // REG_SOUNDxSAD must be word aligned theoretically but since it's a memory pointer, 
    // the DS usually accepts the offset. 
    REG_SOUNDxSAD(ds_ch) = new_sad;

    // If there's ever weird noise when the phase changes, switch to use this instead
    // REG_SOUNDxSAD(ds_ch) = new_sad & ~3;
}

/// @brief               Updates the DS Sound Hardware Channels with the processed data for sound output.
/// @param ch_idx        DS Hardware Sound Channel channel index.
/// @param nes_apu_clock NES APU sound clock speed in Hz.
/// @param ds_sound_freq DS Hardware Sound Frequency in Hz.
static void n163UpdateChannelStatus(int ch_idx, Uint32 nes_apu_clock, Uint32 ds_sound_freq)
{
    u8 ds_ch = n163_ds_channels[ch_idx];
    u8 *regs = &n163s.ram[0x40 + (ch_idx * 8)];
    
    u32 freq_val = regs[0] | (regs[2] << 8) | ((regs[4] & 0x03) << 16);
    u32 length   = 256 - (regs[4] & 0xFC); 
    u8 wave_adr  = regs[6];
    u8 volume    = regs[7] & 0x0F; // Vol is in the 4 LSB

    // Channel shutdown verification
    u8 num_ch = ((n163s.ram[0x7F] >> 4) & 0x07) + 1;
    u8 start_ch = 8 - num_ch;

    if (ch_idx < start_ch || volume == 0 || freq_val == 0)
    {
        if (snd_isChannelPlaying(ds_ch)) snd_stopChannel(ds_ch);
        return;
    }

    // Sample rate calculation.
    // According to nesdev: f = (CPU_CLOCK * freq) / (15 * num_ch * 2^18)
    // But the DS only needs the frequency of a single step from the wave.
    // dsTimer = BusClock / (F_step * 2)
    
    // Combined formula simplification to avoid overflows:
    // ds_timer = (BusClock * 15 * num_ch * 65536) / (CPU_NES * freq)
    u64 num = (u64)DS_BUS_CLOCK * 15 * num_ch * (ds_sound_freq << 1);
    u64 den = (u64)nes_apu_clock * freq_val;
    u32 timer_res = (u32)(num / den);
    
    u16 final_timer = (u16)-(timer_res >> 1);

    // Update DS sound hardware
    REG_SOUNDxSAD(ds_ch) = (u32)&n163_wave_shadow[wave_adr];
    REG_SOUNDxTMR(ds_ch) = final_timer;
    REG_SOUNDxPNT(ds_ch) = 0;
    REG_SOUNDxLEN(ds_ch) = length >> 2;

    // Get each Ch pan value from the table
    if (apu_cfg.stereo)
    {
        n163s.pan = n163_pan_table[ch_idx];
    }
    else
    {
        n163s.pan = 64;
    }

    // Reset the base parameters only (SAD or LEN) if they changed,
    // to avoid metallic clicks by resetting the channel repeatedly.
    u32 control = SOUNDCNT_ENABLED | SOUNDCNT_FORMAT_PCM8 | 
                  SOUNDCNT_MODE_LOOP | SOUNDCNT_PAN(n163s.pan) | 
                  SOUNDCNT_VOLUME(volume << 2); // x4 is the sweet volume spot

    if (REG_SOUNDxCNT(ds_ch) != control || REG_SOUNDxTMR(ds_ch) != final_timer)
    {
        REG_SOUNDxCNT(ds_ch) = control;
    }
}

void __fastcall n163SoundWrite(Uint address, Uint value)
{
    address &= 0x7F;
    if (n163s.ram[address] == value)
    {
        return; // If the value is the same, ignore it.
    }

    n163s.ram[address] = value;

    if (address < 0x40)
    {
        // Update our Wave Ram shadow
        n163UpdateShadowWaveRAM(address, value);
        
        // NOTE: If a wave changes when playing, the DS hardware
        // will reflect it in the next loop cycle. 
        // We don't need to reset the channel.
    }
    else
    {
        // Channel registers ($40-$7F)
        int ch_idx = (address - 0x40) / 8;
        n163s.dirty_channels |= (1 << ch_idx);

        // Special case: The $7F reg changes the freq of ALL channels
        if (address == 0x7F)
        {
            u8 num_ch = ((value >> 4) & 0x07) + 1;
            if (num_ch != n163s.num_ch_old)
            {
                n163s.dirty_channels = 0xFF; // Mark every channel for pitch recalc
                n163s.num_ch_old = num_ch;
            }
        }

        // Detect if a phase change occurs (rare and only used in some custom nsf songs)
        int reg_in_ch = (address - 0x40) % 8;

        // Low: 1, Mid: 3, High: 5
        if (reg_in_ch == 1 || reg_in_ch == 3 || reg_in_ch == 5)
        {
            // Mark a "Resync Phase" flag
            n163s.phase_dirty |= (1 << ch_idx);
        }
    }
}

void n163SoundHwUpdate(Uint32 nes_apu_clock, Uint32 ds_sound_freq)
{
    if (!has_n163) return;

    if (n163s.dirty_channels == 0)
    {
        return;
    }

    for (int i = 0; i < 8; i++)
    {
        if (n163s.dirty_channels & (1 << i))
        {
            n163UpdateChannelStatus(i, nes_apu_clock, ds_sound_freq);
        }
        else if (n163s.phase_dirty & (1 << i))
        {
            n163SyncPhase(i);
        }
    }
    n163s.dirty_channels = 0; // Clean flags
}

void n163SoundHwStop()
{
    // Stops all the 8 channels at once
    for(int i = 0; i < 8; i++)
    {
        snd_stopChannel(n163_ds_channels[i]);
    }
}

void n163SoundInit(Uint32 nes_apu_clock, Uint32 ds_sound_freq)
{
    memset(&n163s, 0, sizeof(N163_STATE));
    memset(n163_wave_shadow, 0, sizeof(n163_wave_shadow));
    n163SoundHwStop();
}
