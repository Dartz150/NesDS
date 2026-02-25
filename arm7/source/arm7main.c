#include <nds.h>
#include <nds/arm7/audio.h>
#include <string.h>
#include "c_defs.h"
#include "audiosys.h"
#include "s_apu.h"
#include "s_defs.h"
#include "s_vrc6.h"
#include "s_fds.h"
#include "s_mmc5.h"

// Information sources:
// - https://www.nesdev.org/wiki/APU_Mixer
// - https://problemkaputt.de/gbatek.htm#dssound
// - https://stackoverflow.com/questions/14997850/fir-filter-implementation-in-c-programming

// DS Hardware Defines

// RIGHT CHANNEL
#define RIGHT_CHANNEL     0
#define R_VOL             SOUND_VOL(127)
#define R_PAN             SOUND_PAN(0)
// LEFT CHANNEL
#define LEFT_CHANNEL      1
#define L_VOL             SOUND_VOL(127)
#define L_PAN             SOUND_PAN(127)

// DS Mixer buffers
#define RING_BUF_SIZE (MIXBUFSIZE << 3) // Buffer needs to be at least 1024 for stability
#define RING_MASK (RING_BUF_SIZE - 1)

static s16 buffer_L[RING_BUF_SIZE] ALIGN(32);
static s16 buffer_R[RING_BUF_SIZE] ALIGN(32);
static s16 temp_buf[MIXBUFSIZE] ALIGN(32); // Intermediate buffer to hold processed samples
static int buff_write_cursor;

// Sound status flags
static uint32_t nes_apu_clock;
static uint32_t ds_sound_freq;
static int APU_paused;

void setDsSoundFreq()
{
	ds_sound_freq = DS_SOUND_FREQUENCY;
}

void setApuRegion()
{
	// VRC6/FDS/MMC5 titles are always NTSC
	if (has_vrc6 || has_fds || has_mmc5)
	{
		nes_apu_clock = NES_APU_NTSC;
	}
	else
	{
		nes_apu_clock = apu_cfg.region_pal 
		? NES_APU_PAL
		: NES_APU_NTSC;
	}
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

__fastcall void readApu()
{
	int max_cmds = 32; // Security limit
    while(fifoCheckValue32(FIFO_USER_07) && max_cmds--) 
	{
        u32 msg = fifoGetValue32(FIFO_USER_07);
        apuSoundWrite(msg >> 8, msg & 0xFF);
    }
    IPC_APUR = IPC_APUW;
}

// Main audio loop.
// By using a Ring Buffer, we prevent sound saturation and audio 
// corruption caused by a clock drift.
void __fastcall soundMain()
{
    if (APU_paused) return;

    // Render a NES Sound frame. Generates the deltas/samples for every APU channel.
    nesApuProcessChannels(MIXBUFSIZE, nes_apu_clock, ds_sound_freq);

	// blip_buf already converts deltas to centered PCM16 samples, prefect for the DS
	// Reading ensures blip_buf internal avail stays in sync with the timers.
    int read = blip_read_samples(master_blip, temp_buf, MIXBUFSIZE, 0);
	ptr_mixed = 0; // Always reset read pointer

	// Fill with silence if blip_buf underflows to avoid playing old buffer data
    if (read < MIXBUFSIZE)
	{
        memset(temp_buf + read, 0, (MIXBUFSIZE - read) * sizeof(s16));
    }

	// Add Sound Expansion samples if enabled
    if (has_fds && !apu_cfg.fds)
	{
        for (int i = 0; i < MIXBUFSIZE; i++)
		{
			// Get FDS samples from the render
            int32_t fds_sample = FDSSoundRender();
			// Apply gain consistent with the nes APU
            temp_buf[i] = clampSample16((int32_t)temp_buf[i] + (fds_sample << 1));
        }
    }

	// The Sound hardware is now independently looping through buffer_L/R
    for (int i = 0; i < MIXBUFSIZE; i++)
	{
        buffer_L[buff_write_cursor] = temp_buf[i];
        buffer_R[buff_write_cursor] = temp_buf[i]; // Stereo copy TODO: Add pseudo-stereo effect back
        buff_write_cursor = (buff_write_cursor + 1) & RING_MASK;
    }

    readApu();
    APU4015Reg();
}

static void clearSoundBuffers(void)
{
    memset(buffer_L, 0, sizeof(buffer_L));
    memset(buffer_R, 0, sizeof(buffer_R));
	memset(temp_buf, 0, sizeof(temp_buf));
	buff_write_cursor = 0;
	if (master_blip)
	{
        blip_clear(master_blip);
    }
}

void initsound()
{
	powerOn(BIT(0));
	REG_SOUNDCNT = SOUND_ENABLE | SOUND_VOL(127);

	SCHANNEL_SOURCE(RIGHT_CHANNEL) = (u32)buffer_R;
	SCHANNEL_SOURCE(LEFT_CHANNEL)  = (u32)buffer_L;

	SCHANNEL_TIMER(RIGHT_CHANNEL) = TIMER_NFREQ; // 32768Hz
	SCHANNEL_TIMER(LEFT_CHANNEL)  = TIMER_NFREQ;

	SCHANNEL_LENGTH(RIGHT_CHANNEL) = sizeof(buffer_R) >> 2;
	SCHANNEL_LENGTH(LEFT_CHANNEL)  = sizeof(buffer_L) >> 2;

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

	TIMER_DATA(0) = TIMER_NFREQ << 1;
	TIMER_CR(0) = TIMER_ENABLE;

	TIMER_DATA(1) = (u16)-MIXBUFSIZE;
	TIMER_CR(1) = TIMER_CASCADE | TIMER_IRQ_REQ | TIMER_ENABLE;
	nesApuSoundHwStop();
}

void stopsound()
{
	TIMER_CR(0) = 0;
    TIMER_CR(1) = 0;
	SCHANNEL_CR(RIGHT_CHANNEL) &= ~SCHANNEL_ENABLE;
    SCHANNEL_CR(LEFT_CHANNEL)  &= ~SCHANNEL_ENABLE;
    nesApuSoundHwStop();
	clearSoundBuffers();
}

void restartsound()
{
	soundMain();
	
	SCHANNEL_CR(RIGHT_CHANNEL) |= SCHANNEL_ENABLE;
    SCHANNEL_CR(LEFT_CHANNEL)  |= SCHANNEL_ENABLE;

	TIMER_CR(0) = TIMER_ENABLE; 
	TIMER_CR(1) = TIMER_CASCADE | TIMER_IRQ_REQ | TIMER_ENABLE;
}

// Stops sound, restarts sound, reset apu, refreshes 4015 reg, clears buffer
void lidinterrupt(void)
{
	stopsound();
	restartsound();
	clearSoundBuffers();
}

// Reinits the whole APU + Sound expansions
void resetApu()
{
	// Only detect expansions once per reset
    const int mapper = IPC_MAPPER;
    has_vrc6 = (mapper == 24 || mapper == 26 || mapper == 256);
    has_fds  = (mapper == 20 || mapper == 256);
	has_mmc5 = (mapper == 5 || mapper == 256);

	clearSoundBuffers();
	setApuRegion();
	apuSoundInit(nes_apu_clock, ds_sound_freq);
	if (has_vrc6)
	{
		vrc6SoundInit();
	}

	if (has_fds)
	{
		fdsSoundInit(nes_apu_clock, ds_sound_freq);
	}
	
	if (has_mmc5)
	{
		mmc5SoundInit();
	}
	IPC_APUW = 0;
	IPC_APUR = 0;
}

void soundinterrupt(void)
{
	soundMain(); 
    REG_IF = IRQ_TIMER1;
}

void fifointerrupt(u32 msg, void *none) // This should be registered to a fifo channel.
{
	u32 cmd = msg & 0xFF;
    u32 data = msg >> 8; // APU Status flags Cfg bits
	switch(cmd)
	{
		case FIFO_APU_UPDATE_FLAGS:
            applyApuStateMask(data);
            break;
		case FIFO_APU_PAUSE:
			APU_paused = 1;
			clearSoundBuffers();
			nesApuSoundHwStop();
			break;
		case FIFO_UNPAUSE:
			APU_paused = 0;
			break;
		case FIFO_APU_RESET:
			clearSoundBuffers();
			nesApuSoundHwStop();
			APU_paused = 0;
			resetApu();
			APU4015Reg();
			readApu();
			break;
		case FIFO_SOUND_RESET:
			lidinterrupt();
			break;
		case FIFO_SOUND_UPDATE:
			readApu();
			APU4015Reg();
			break;
	}
}

void interrupthandler() 
{
	u32 flags = REG_IF&REG_IE;

	if (flags&IRQ_TIMER1)
	{
		soundinterrupt();
	}		
}

void nesmain() 
{
	setDsSoundFreq();
	
	resetApu();

	initsound();
	restartsound();

	fifoSetValue32Handler(FIFO_USER_08, fifointerrupt, 0); // Use the last IPC channel to comm..
	irqSet(IRQ_LID, lidinterrupt);
	irqSet(IRQ_TIMER1, soundinterrupt);
	swiWaitForVBlank();
}
