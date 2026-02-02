#include <nds.h>
#include <nds/arm7/audio.h>
#include <string.h>
#include "c_defs.h"
#include "SoundIPC.h"
#include "audiosys.h"
#include "handler.h"
#include "s_apu.h"
#include "s_defs.h"
#include "s_vrc6.h"
#include "s_fds.h"

#define MIXBUFSIZE        (1 << 8)
#define STEREO_DELAY_SIZE 256 // 15ms @ 32kHz
/*
 * DS Hardware Constants (from No$gba DS Sound docs: Channel/Mixer Bit-Widths section)
 * DS expects signed 16-bit PCM (SOUNDxCNT Format=1: PCM16, range -32768 to +32767).
 * We center post-mixer to bipolar for full dynamic range.
 */
#define DC_OFFSET         16384  // Half of 32768: Centers unipolar NES output (0-32767) to bipolar (-16384 to +16383).

// RIGHT CHANNEL
#define RIGHT_CHANNEL     0
#define R_VOL             SOUND_VOL(127)
#define R_PAN             SOUND_PAN(0)
// LEFT CHANNEL
#define LEFT_CHANNEL      1
#define L_VOL             SOUND_VOL(127)
#define L_PAN             SOUND_PAN(127)

// Information sources:
// - https://www.nesdev.org/wiki/APU_Mixer
// - https://problemkaputt.de/gbatek.htm#dssound
// - https://stackoverflow.com/questions/14997850/fir-filter-implementation-in-c-programming

// NES APU Mixer Lookup Tables
// Pulse table: For sum of two pulse channels (0-30 unipolar levels).
// Derived from: 95.52 / (8128.0 / n + 100) for n=1..30, scaled to 0-32767 (Q15 unipolar).
static const int16_t pulse_table[31] =
{
    0, 380, 752, 1114, 1468, 1814, 2152, 2482, 2805, 3120,
    3429, 3731, 4027, 4316, 4599, 4876, 5148, 5414, 5675, 5930,
    6181, 6426, 6667, 6903, 7135, 7363, 7586, 7805, 8020, 8231,
    8438
};

// TND table: For weighted sum of triangle/noise/DMC (0-202 unipolar equivalent levels).
// Approximated from: 163.67 / (24329.0 / n + 100) for n=1..202, scaled to 0-32767.
// Weights (3*tri + 2*noi + dmc)
static const int16_t tnd_table[203] =
{
    0, 220, 437, 653, 867, 1080, 1291, 1500, 1707, 1913,
    2117, 2320, 2521, 2720, 2918, 3115, 3309, 3503, 3695, 3885,
    4074, 4261, 4448, 4632, 4816, 4997, 5178, 5357, 5535, 5712,
    5887, 6061, 6234, 6406, 6576, 6745, 6913, 7080, 7245, 7409,
    7573, 7735, 7895, 8055, 8214, 8371, 8528, 8683, 8838, 8991,
    9143, 9294, 9444, 9593, 9742, 9889, 10035, 10180, 10324, 10467,
    10610, 10751, 10892, 11031, 11170, 11308, 11444, 11580, 11715, 11850,
    11983, 12116, 12247, 12378, 12508, 12637, 12766, 12893, 13020, 13146,
    13271, 13396, 13519, 13642, 13765, 13886, 14007, 14127, 14246, 14364,
    14482, 14599, 14716, 14831, 14946, 15061, 15175, 15288, 15400, 15512,
    15623, 15733, 15843, 15952, 16060, 16168, 16275, 16382, 16488, 16594,
    16698, 16803, 16906, 17009, 17112, 17214, 17315, 17416, 17516, 17616,
    17715, 17814, 17912, 18009, 18106, 18203, 18299, 18394, 18489, 18583,
    18677, 18771, 18863, 18956, 19048, 19139, 19230, 19321, 19411, 19500,
    19589, 19678, 19766, 19853, 19941, 20027, 20114, 20200, 20285, 20370,
    20455, 20539, 20623, 20706, 20789, 20871, 20953, 21035, 21116, 21197,
    21277, 21357, 21437, 21516, 21595, 21674, 21752, 21829, 21907, 21984,
    22060, 22136, 22212, 22288, 22363, 22438, 22512, 22586, 22660, 22733,
    22806, 22879, 22951, 23023, 23095, 23166, 23237, 23307, 23378, 23448,
    23517, 23587, 23656, 23724, 23793, 23861, 23929, 23996, 24063, 24130,
    24197, 24263, 24329
};

// DS Mixer buffers
static s16 buffer_L[MIXBUFSIZE * 2] ALIGN(32);
static s16 buffer_R[MIXBUFSIZE * 2] ALIGN(32);
static int16_t delay_line[STEREO_DELAY_SIZE] ALIGN(32);

// APU mixer status flags (TODO: Move to audiosys.c)
enum apuRegion apuCurrentRegion = NTSC; // Set Flag for the APU settings to match PAL Sound Frequency
enum pulseCycles pulseCurrentStatus = Normal; // SWAP DUTY CYCLES
enum pulseMode CurrentPulseMode = PULSE_CH_SW; // Change pulse 1/2 renderer

// Sound Expansion flags
static bool has_vrc6 = false;
static bool has_fds  = false;

// Sound status flags
static bool stereo_enhanced = true;
static int delay_ptr = 0;
static int APU_paused = 0;
static int chan = 0;

void setPulseModeSw()
{
    CurrentPulseMode = PULSE_CH_SW;
}

void setPulseModeHw()
{
    CurrentPulseMode = PULSE_CH_HW;
}

enum pulseMode getPulseMode()
{
    return CurrentPulseMode;
}

void setApuPal()
{
	apuCurrentRegion = PAL;
}

void setApuNtsc()
{
	apuCurrentRegion = NTSC;
}

enum apuRegion getApuCurrentRegion()
{
	return apuCurrentRegion;
}

void setPulseSwap()
{
	pulseCurrentStatus = Reverse;
}

void setPulseNormal()
{
	pulseCurrentStatus = Normal;
}

enum pulseCycles getPulseCurrentStatus()
{
	return pulseCurrentStatus;
}

// Resets the APU emulation to avoid garbage sounds
void resetApu()
{
	NESReset();
	IPC_APUW = 0;
	IPC_APUR = 0;
	// Only detect expansions once per reset
    const int mapper = IPC_MAPPER;
    has_vrc6 = (mapper == 24 || mapper == 26 || mapper == 256);
    has_fds  = (mapper == 20 || mapper == 256);
}

// https://github.com/Gericom/GBARunner3/blob/develop/code/core/arm7/source/Sound/GbaSound7.c#L50
// Clamps samples to a 16-bit range to prevent overflows in the DS mixer.
__inline static int16_t clampSample16(int32_t inSample)
{
    // For a 16 bit range (-32768 to 32767)
    int32_t outSample = inSample << 16;
    if (inSample != (outSample >> 16))
        outSample = 0x7FFFFFFF ^ (inSample >> 31);
    return (int16_t)(outSample >> 16);
}

//Render the NES APU channels and emulate the NES APU mixer (NESDev wiki: APU Mixer).
__inline static int32_t nesApuSoundRender() 
{
    int32_t pulse = 0;
	// Pulse channels: Render via SW table or skip if using DS PSG Hardware
    if (CurrentPulseMode == PULSE_CH_SW)
	{
        pulse = pulse_table[nesApuSoundPulseRender1() + 
							nesApuSoundPulseRender2()];
    }
    // TND: Weighted sum of Triangle, Noise, and DMC (always Software)
    int32_t tnd = tnd_table[(3 * nesApuSoundTriangleRender1()) + 
                           (2 * nesApuSoundNoiseRender1()) + 
                           nesApuSoundDmcRender1()];
    // Mix 2A03 APU
    int32_t s_apu = pulse + tnd;

	// Add Sound Expansions
    if (has_vrc6)
	{
		s_apu += VRC6SoundRender();
	}
	if (has_fds)
	{
		s_apu += FDSSoundRender();
	}
    return s_apu;
}

// Converts unipolar (0 to 32767) NES levels to bipolar (-16384 to 16383) DS PCM16 samples.
// Centers the waveform to prevent artifacts in the DS mixer.
__inline static int16_t nesToDsSample(int32_t raw_sample)
{
    int32_t mixed = ((raw_sample - DC_OFFSET) * 3) >> 1; // Apply linear gain (1.5x factor is the sweet spot).
    return clampSample16(mixed);
}

// Applies sound post-processing
__inline static void applySoundPostProcessing(int16_t sample, int16_t *outL, int16_t *outR, int *ptr) 
{
    if (!stereo_enhanced) 
	{
		// Normal mono sound
        *outL = sample;
        *outR = sample;
        return;
    }

    // R Channel: Original Sample
    *outR = sample;

    // Obtain delayed sample for L
	int current_ptr = *ptr;
    int16_t delayed = delay_line[current_ptr];

    // Store our current sample in the delay line
    delay_line[current_ptr] = sample;
    *ptr = (current_ptr + 1) % STEREO_DELAY_SIZE;

    // Phase inverted Channel L for a surround pseudo-stereo effect.
    int32_t left_mix = sample - (delayed >> 1); // 50% vol
    *outL = clampSample16(left_mix);
}

void __fastcall soundMain(int chan)
{
    if (APU_paused) return;

    s16 *pcmL = &buffer_L[chan * MIXBUFSIZE];
    s16 *pcmR = &buffer_R[chan * MIXBUFSIZE];
	int local_delay_ptr = delay_ptr;

    for (int i = 0; i < MIXBUFSIZE; i++) 
	{
        // Get NES sound samples
        int32_t nes_sample = nesApuSoundRender();

        // Convert NES samples to PCM16
        int16_t ds_sample = nesToDsSample(nes_sample);

        // Apply post-processing effects and write to the audio buffers
        applySoundPostProcessing(ds_sample, pcmL++, pcmR++, &local_delay_ptr);
    }
	delay_ptr = local_delay_ptr;

    // Process Hardware PSG when enabled
    if (CurrentPulseMode == PULSE_CH_HW)
	{
		nesApuSoundPulseHwRender();
	}
	// Sync APU logic and registers
    readApu();
    APU4015Reg();
}

static void clearSoundBuffers(void)
{
    memset(delay_line, 0, sizeof(delay_line));
    memset(buffer_L, 0, sizeof(buffer_L));
    memset(buffer_R, 0, sizeof(buffer_R));
    delay_ptr = 0;
}

void initsound()
{
	powerOn(BIT(0));
	REG_SOUNDCNT = SOUND_ENABLE | SOUND_VOL(127);

    u16 timerVal = TIMER_NFREQ; // 32768Hz

	SCHANNEL_SOURCE(RIGHT_CHANNEL) = (u32)&buffer_R[0];
	SCHANNEL_SOURCE(LEFT_CHANNEL) = (u32)&buffer_L[0];

	SCHANNEL_TIMER(RIGHT_CHANNEL) = timerVal;
	SCHANNEL_TIMER(LEFT_CHANNEL) = timerVal;

	SCHANNEL_LENGTH(RIGHT_CHANNEL) = MIXBUFSIZE;
	SCHANNEL_LENGTH(LEFT_CHANNEL) = MIXBUFSIZE;

	SCHANNEL_REPEAT_POINT(RIGHT_CHANNEL) = 0;
	SCHANNEL_REPEAT_POINT(LEFT_CHANNEL) = 0;

	SCHANNEL_CR(RIGHT_CHANNEL) =
		SOUND_REPEAT |
		R_VOL |
		R_PAN |
		SOUND_FORMAT_16BIT;
	SCHANNEL_CR(LEFT_CHANNEL) =
		SOUND_REPEAT |
		L_VOL |
		L_PAN |
		SOUND_FORMAT_16BIT;

	TIMER_DATA(0) = timerVal << 1;
	TIMER_CR(0) = TIMER_ENABLE;

	TIMER_DATA(1) = (u16)-MIXBUFSIZE;
	TIMER_CR(1) = TIMER_CASCADE | TIMER_IRQ_REQ | TIMER_ENABLE;
	nesApuSoundPulseHwStop();
}

void stopsound()
{
	TIMER_CR(0) = 0;
    TIMER_CR(1) = 0;
	SCHANNEL_CR(RIGHT_CHANNEL) &= ~SCHANNEL_ENABLE;
    SCHANNEL_CR(LEFT_CHANNEL)  &= ~SCHANNEL_ENABLE;
    nesApuSoundPulseHwStop();
	clearSoundBuffers();
}

void restartsound(int ch)
{
	chan = ch;

	SCHANNEL_CR(RIGHT_CHANNEL) |= SCHANNEL_ENABLE;
    SCHANNEL_CR(LEFT_CHANNEL)  |= SCHANNEL_ENABLE;

	TIMER_CR(0) = TIMER_ENABLE; 
	TIMER_CR(1) = TIMER_CASCADE | TIMER_IRQ_REQ | TIMER_ENABLE;
}

// Stops sound, restarts sound, reset apu, refreshes 4015 reg, clears buffer
void lidinterrupt(void)
{
	stopsound();
	restartsound(1);
}

void soundinterrupt(void)
{
	chan^=1;
	soundMain(chan);
	if(REG_IF & IRQ_TIMER1)
	{
		lidinterrupt();
		chan = 1;
		REG_IF = IRQ_TIMER1;
	}

}

void fifointerrupt(u32 msg, void *none)			//This should be registered to a fifo channel.
{
	switch(msg&0xff) 
	{
		case FIFO_APU_PAUSE:
			APU_paused = 1;
			clearSoundBuffers();
			nesApuSoundPulseHwStop();
			break;
		case FIFO_UNPAUSE:
			APU_paused = 0;
			break;
		case FIFO_APU_RESET:
			clearSoundBuffers();
			nesApuSoundPulseHwStop();
			APU_paused = 0;
			resetApu();
			APU4015Reg();
			readApu();
			break;
		case FIFO_SOUND_RESET:
			lidinterrupt();
			break;
		case FIFO_APU_PAL:
			setApuPal();
			readApu();
			break;
		case FIFO_APU_NTSC:
			setApuNtsc();
			readApu();
			break;
		case FIFO_APU_SWAP:
			setPulseSwap();
			readApu();
			break;
		case FIFO_APU_NORM:
			setPulseNormal();
			readApu();
			break;
		case FIFO_SOUND_UPDATE:
			readApu();
			APU4015Reg();
			break;
		case FIFO_APU_PULSE_SW:
            setPulseModeSw();
            break;
        case FIFO_APU_PULSE_HW:
            setPulseModeHw();
            break;
		case FIFO_APU_STEREO_ON:
			stereo_enhanced = true;
			break;
		case FIFO_APU_STEREO_OFF:
			stereo_enhanced = false;
			delay_ptr = 0;
    break;
	}
}

void readApu()
{
	int max_cmds = 32; // Security limit
    while(fifoCheckValue32(FIFO_USER_07) && max_cmds--) 
	{
        u32 msg = fifoGetValue32(FIFO_USER_07);
        apuSoundWrite(msg >> 8, msg & 0xFF);
    }
    IPC_APUR = IPC_APUW;
}

void interrupthandler() 
{
	u32 flags=REG_IF&REG_IE;
	if(flags&IRQ_TIMER1)
		soundinterrupt();
}

void nesmain() 
{
	clearSoundBuffers();
	apuSoundInstall();
	VRC6SoundInstall();
	FDSSoundInstall();
	
	resetApu();

	initsound();
	restartsound(1);

	fifoSetValue32Handler(FIFO_USER_08, fifointerrupt, 0);		//use the last IPC channel to comm..
	irqSet(IRQ_LID, lidinterrupt);
	irqSet(IRQ_TIMER1, soundinterrupt);
	swiWaitForVBlank();
}
