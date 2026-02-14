#include <nds.h>
#include <nds/arm7/audio.h>
#include <string.h>
#include "c_defs.h"
#include "audiosys.h"
#include "handler.h"
#include "s_apu.h"
#include "s_defs.h"
#include "s_vrc6.h"
#include "s_fds.h"

// DS Hardware Defines

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

// DS Mixer buffers
static s16 buffer_L[MIXBUFSIZE * 2] ALIGN(32);
static s16 buffer_R[MIXBUFSIZE * 2] ALIGN(32);

// Sound status flags
static int APU_paused;
static int chan;

// Resets the APU emulation to avoid garbage sounds
void resetApu()
{
	// Only detect expansions once per reset
    const int mapper = IPC_MAPPER;
    has_vrc6 = (mapper == 24 || mapper == 26 || mapper == 256);
    has_fds  = (mapper == 20 || mapper == 256);
	NESReset();
	IPC_APUW = 0;
	IPC_APUR = 0;
}

// blip_buf mixes everything, we no longer need to emulate the APU mixer or convert samples.
void __fastcall soundMain(int active_chan)
{
    if (APU_paused) return;

	s16 *pcmL = &buffer_L[active_chan * MIXBUFSIZE];
    s16 *pcmR = &buffer_R[active_chan * MIXBUFSIZE];
    
    // Render NES Sound frame. blip_buf already delivers centered PCM16 samples, prefect for the DS
    nesApuProcessBlipBufferChannels(MIXBUFSIZE, pcmL);

    // Fill Buffers for the DS hardware (TODO: Handle filter and stereo using blip_buf)
    memcpy(pcmR, pcmL, MIXBUFSIZE * sizeof(s16));

    readApu();
    APU4015Reg();
}

static void clearSoundBuffers(void)
{
    memset(buffer_L, 0, sizeof(buffer_L));
    memset(buffer_R, 0, sizeof(buffer_R));
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

void restartsound(int ch)
{
	soundMain(0);
    soundMain(1);

	chan = 0;
	
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
    soundMain(chan); 
    chan ^= 1; 
    REG_IF = IRQ_TIMER1;
}

void fifointerrupt(u32 msg, void *none)			//This should be registered to a fifo channel.
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
	restartsound(0);

	fifoSetValue32Handler(FIFO_USER_08, fifointerrupt, 0);		//use the last IPC channel to comm..
	irqSet(IRQ_LID, lidinterrupt);
	irqSet(IRQ_TIMER1, soundinterrupt);
	swiWaitForVBlank();
}
