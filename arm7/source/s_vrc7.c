#include <math.h>
#include <string.h>
#include "nestypes.h"
#include "audiosys.h"

/* * VRC7 Audio Engine attempt for the DS ARM7

 * This is an initial unfinished and over simplified implementation of the VRC7
 * NES sound expansion (YM2413) for the Nintendo DS.
 * It uses a linear sine wave approach instead of the original log2sin/exponential tables 
 * to reduce complexity on the ARM7.
 *
 * KNOWN LIMITATIONS:
 * - Rough ADSR, this uses a simplified FDS envelope, similar to the approach in s_fds.c
 * - There's a lot of "zipper" noise, I haven't found why yet, but maybe because volume changes are not smoothed.
 * - Linear phase modulation is a rough approximation of the original hardware, which needs complex calcs to achieve.
 */

#define PHASE_BITS      18
#define PHASE_MASK      ((1 << PHASE_BITS) - 1)
#define SINE_BITS       8
#define SINE_MASK       ((1 << SINE_BITS) - 1)
#define VRC7_ENV_BITS   12

typedef struct
{
    u32 m_phase;
    u32 c_phase;
    u32 m_step;         // Pre-calculated phase step
    u32 c_step;         // Pre-calculated phase step
    s32 mod_last_out;   // Feedback storage
    
    s32 env_vol;        // Current volume
    u32 env_cnt;        // Speed counter
    u32 env_spd;        // Envelope speed

    bool active;
    
    // Cache parameters to avoid recalculations
    u8 last_inst, last_vol, last_freq_l, last_reg;
} VRC7_VOICE;

static s16 fm_sine_lut[257]; // 256 + 1 for linear interpolation guard point
static const u8 vrc7_mult_lut[16] = {1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30};
static u8 custom_patch[8];
static VRC7_VOICE voices[6];
static u8 vrc7_freq_low[6], vrc7_channel_reg[6], vrc7_inst_vol[6];

// Hardcoded VRC7 internal patches
static const u8 vrc7_patches[16][8] =
{
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // Custom Patch
    {0x03,0x21,0x05,0x06,0xE8,0x81,0x42,0x27}, // Buzzy Bell
    {0x13,0x41,0x14,0x0D,0xD8,0xF6,0x23,0x12}, // Guitar
    {0x11,0x11,0x08,0x08,0xFA,0xB2,0x20,0x12}, // Wurly
    {0x31,0x61,0x0C,0x07,0xA8,0x64,0x61,0x27}, // Flute
    {0x32,0x21,0x1E,0x06,0xE1,0x76,0x01,0x28}, // Clarinet
    {0x02,0x01,0x06,0x00,0xA3,0xE2,0xF4,0xF4}, // Synth
    {0x21,0x61,0x1D,0x07,0x82,0x81,0x11,0x07}, // Trumpet
    {0x23,0x21,0x22,0x17,0xA2,0x72,0x01,0x17}, // Organ
    {0x35,0x11,0x25,0x00,0x40,0x73,0x72,0x01}, // Bells
    {0xB5,0x01,0x0F,0x0F,0xA8,0xA5,0x51,0x02}, // Vibes
    {0x17,0xC1,0x24,0x07,0xF8,0xF8,0x22,0x12}, // Vibraphone
    {0x71,0x23,0x11,0x06,0x65,0x74,0x18,0x16}, // Tutti
    {0x01,0x02,0xD3,0x05,0xC9,0x95,0x03,0x02}, // Fretless
    {0x61,0x63,0x0C,0x00,0x94,0xC0,0x33,0xF6}, // Synth Bass
    {0x21,0x72,0x0D,0x00,0xC1,0xD5,0x56,0x06}  // Sweep
};

// Fast Linear Interpolation for the Sine Table, reduces metallic noise without large LUTs
static inline s32 interpolate_sine(u32 phase)
{
    u32 idx = (phase >> PHASE_BITS) & 0xFF;
    u32 fract = (phase >> (PHASE_BITS - 8)) & 0xFF; 
    
    s32 s1 = fm_sine_lut[idx];
    s32 s2 = fm_sine_lut[idx + 1];
    
    return s1 + (((s2 - s1) * (s32)fract) >> 8);
}


// Update frequency steps per channel, TODO: move to fixed-point.
static void update_voice_params(int ch)
{
    VRC7_VOICE *v = &voices[ch];
    u8 inst_idx = (vrc7_inst_vol[ch] >> 4) & 0x0F;
    const u8* p = (inst_idx == 0) ? custom_patch : vrc7_patches[inst_idx];
    
    u16 freq = vrc7_freq_low[ch] | ((vrc7_channel_reg[ch] & 0x01) << 8);
    u8 octave = (vrc7_channel_reg[ch] >> 1) & 0x07;
    
    // Scale factor for 18-bit phase at 32kHz sampling rate
    float f_step = (float)freq * (float)(1 << octave) * 0.189635f;
    u32 base_step = (u32)(f_step * (float)(1 << 9)); 

    v->m_step = (base_step * vrc7_mult_lut[p[0] & 0x0F]);
    v->c_step = (base_step * vrc7_mult_lut[p[1] & 0x0F]);
}

// Called in arm7main.c
int32_t __fastcall vrc7SoundRender(void)
{
    s32 mixed_out = 0;

    for (int ch = 0; ch < 6; ch++)
    {
        VRC7_VOICE *v = &voices[ch];
        
        // --- Simplified Envelope Logic ---
        bool trigger = (vrc7_channel_reg[ch] >> 4) & 1;
        v->env_cnt += (1 << VRC7_ENV_BITS); 
        if (v->env_cnt >= v->env_spd)
        {
            v->env_cnt = 0;
            if (trigger)
            {
                // Pseudo Attack (asymptotic approach to max volume)
                v->env_vol += (1024 - v->env_vol) >> 6;
                v->active = true;
            }
            else
            {
                // Linear Release
                if (v->env_vol > 0)
                {
                    v->env_vol -= 4;
                } 
                else
                {
                    v->active = false;
                }
            }
        }

        if (!v->active)
        {
            continue;
        }

        update_voice_params(ch);

        const u8 inst_idx = (vrc7_inst_vol[ch] >> 4) & 0x0F;
        const u8* p = (inst_idx == 0) ? custom_patch : vrc7_patches[inst_idx];

        // --- FM Synthesis Core ---
        int mod_level = 63 - (p[2] & 0x3F);
        int feedback_shift = p[3] & 0x07;
        int vol_level = 15 - (vrc7_inst_vol[ch] & 0x0F);

        // Modulator with Feedback
        s32 fb = (feedback_shift > 0) ? (v->mod_last_out >> (9 - feedback_shift)) : 0;
        u32 m_phase_mod = v->m_phase + (fb << PHASE_BITS);
        s32 s_mod = interpolate_sine(m_phase_mod);
        
        // Half wave rectification for modulator
        if (((p[3] >> 4) & 1) && s_mod < 0)
        {
            s_mod = 0;
        }
        v->mod_last_out = (s_mod * mod_level) >> 6;

        // Carrier modulated by operator 1
        u32 c_phase_mod = v->c_phase + (v->mod_last_out << PHASE_BITS);
        s32 s_car = interpolate_sine(c_phase_mod);
        
        // Half wave rectification for carrier
        if (((p[3] >> 3) & 1) && s_car < 0)
        {
            s_car = 0;
        }

        // Final mix with channel and envelope volume
        mixed_out += (s_car * vol_level * v->env_vol) >> 14;

        // Phase accumulation
        v->m_phase += v->m_step;
        v->c_phase += v->c_step;
    }
    return mixed_out << 5; // Scale to the DS mixer vol amplitude
}

// Called in s_apu.c
void vrc7SoundWrite(Uint address, Uint value)
{
    u8 reg = address & 0x7F;
    if (reg <= 0x07)
    {
        custom_patch[reg] = (u8)value;
    }
    else if (reg >= 0x10 && reg <= 0x15)
    {
        vrc7_freq_low[reg - 0x10] = (u8)value;
    }
    else if (reg >= 0x20 && reg <= 0x25)
    {
        vrc7_channel_reg[reg - 0x20] = (u8)value;
    }
    else if (reg >= 0x30 && reg <= 0x35)
    {
        vrc7_inst_vol[reg - 0x30] = (u8)value;
    }
}

// Called in arm7main.c
void vrc7SoundInit()
{
    memset(voices, 0, sizeof(voices));
    // Precalculate sine table with a small amplitude to prevent overflow in later steps
    for(int i = 0; i < 256; i++)
    {
        fm_sine_lut[i] = (s16)(sin(i * 2.0 * 3.14159265f / 256.0f) * 127.0f);
    }
    fm_sine_lut[256] = fm_sine_lut[0]; 
    for(int i = 0; i < 6; i++)
    {
        voices[i].env_spd = 20 << VRC7_ENV_BITS; 
        voices[i].env_vol = 0;
        voices[i].env_cnt = 0;
    }
}