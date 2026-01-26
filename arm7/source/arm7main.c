#include <nds.h>
#include <nds/arm7/audio.h>
#include <string.h>
#include "c_defs.h"
#include "SoundIPC.h"
#include "audiosys.h"
#include "handler.h"
#include "s_defs.h"
#include "s_vrc6.h"

// Information sources:
// - https://www.nesdev.org/wiki/APU_Mixer
// - https://problemkaputt.de/gbatek.htm#dssound
// - https://stackoverflow.com/questions/14997850/fir-filter-implementation-in-c-programming

// NES APU Mixer Lookup Tables
// Pulse table: For sum of two pulse channels (0-30 unipolar levels).
// Derived from: 95.52 / (8128.0 / n + 100) for n=1..30, scaled to 0-32767 (Q15 unipolar).
const int16_t pulse_table[31] =
{
    0, 380, 752, 1114, 1468, 1814, 2152, 2482, 2805, 3120,
    3429, 3731, 4027, 4316, 4599, 4876, 5148, 5414, 5675, 5930,
    6181, 6426, 6667, 6903, 7135, 7363, 7586, 7805, 8020, 8231,
    8438
};

// TND table: For weighted sum of triangle/noise/DMC (0-202 unipolar equivalent levels).
// Approximated from: 163.67 / (24329.0 / n + 100) for n=1..202, scaled to 0-32767.
// Weights (3*tri + 2*noi + dmc)
const int16_t tnd_table[203] =
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

s16 buffer [MIXBUFSIZE * 2];

enum ApuRegion ApuCurrentRegion = NTSC; // Set Flag for the APU settings to match PAL Sound Frequency
enum ApuCycles ApuCurrentStatus = Normal; // SWAP DUTY CYCLES
enum PulseMode CurrentPulseMode = PULSE_CH_SW; // Change pulse 1/2 renderer

void SetPulseModeSW() 
{
    CurrentPulseMode = PULSE_CH_SW;
}

void SetPulseModeHW() 
{
    CurrentPulseMode = PULSE_CH_HW;
}

enum PulseMode GetPulseMode() 
{
    return CurrentPulseMode;
}

void SetApuPAL()
{
	ApuCurrentRegion = PAL;
}

void SetApuNTSC()
{
	ApuCurrentRegion = NTSC;
}

enum ApuRegion getApuCurrentRegion()
{
	return ApuCurrentRegion;
}

void SetApuSwap()
{
	ApuCurrentStatus = Reverse;
}

void SetApuNormal()
{
	ApuCurrentStatus = Normal;
}

enum ApuCycles getApuCurrentStatus()
{
	return ApuCurrentStatus;
}

// Resets the APU emulation to avoid garbage sounds
void resetAPU() 
{
	NESReset();
	IPC_APUW = 0;
	IPC_APUR = 0;
}

int pcmpos = 0;
int APU_paused = 0;

// Simple clamp for saturation (prevents overflow in DS mixer).
static inline int16_t clampSamples16(int32_t val) 
{
    return (val > MAX_S16) 
		? MAX_S16 
		: ((val < MIN_S16) 
			? MIN_S16 
			: (int16_t)val);
}

static int chan = 0;
int ENBLD = SCHANNEL_ENABLE;
int RPEAT = SOUND_REPEAT;
int PCM_8 = SOUND_FORMAT_8BIT;
int PCM16 = SOUND_FORMAT_16BIT;
int ADPCM = SOUND_FORMAT_ADPCM;

#define U7_MIN   0x00
#define U7_MAX   0x7F    // 127

#define U7_PCT(pct)   (((pct) * U7_MAX + 50) / 100)

#define U7_0_PCT     U7_PCT(0)     // 0
#define U7_25_PCT    U7_PCT(25)    // 32
#define U7_50_PCT    U7_PCT(50)    // 64
#define U7_75_PCT    U7_PCT(75)    // 95
#define U7_100_PCT   U7_PCT(100)   // 127

//RIGHT
int R_VOL = SOUND_VOL(U7_100_PCT);
int R_PAN = SOUND_PAN(U7_0_PCT);
// LEFT
int L_VOL = SOUND_VOL(U7_100_PCT);
int L_PAN = SOUND_PAN(U7_100_PCT);

// This emulates the NES APU mixer (NESDev wiki: APU Mixer).
// It converts unipolar NES levels to bipolar DS PCM16 samples.
void __fastcall mix(int chan)
{
    if (APU_paused) return;
    s16 *pcmBuffer = &buffer[chan * MIXBUFSIZE];
    const int mapper = IPC_MAPPER;
    const bool vrc6 = (mapper == 24 || mapper == 26 || mapper == 256);

    for (int i = 0; i < MIXBUFSIZE; i++) 
    {
		int32_t pulse = 0;
		// Pulse channels: Render via SW table or skip if using DS PSG Hardware
        if (CurrentPulseMode == PULSE_CH_SW)
		{
            pulse = pulse_table[NESAPUSoundSquareRender1() + NESAPUSoundSquareRender2()];
        }
		// TND: Weighted sum of Triangle, Noise, and DMC (always Software)
        int32_t tnd   = tnd_table[(3 * NESAPUSoundTriangleRender1()) + 
                                  (2 * NESAPUSoundNoiseRender1()) + 
                                  NESAPUSoundDpcmRender1()];
		// Mix 2A03 APU			  
        int32_t s_apu = (pulse + tnd);

		// VRC6 Expansion:
        if (vrc6) 
		{
            s_apu += VRC6SoundRender();
        }
		// Convert unipolar (0 to 32767) to bipolar (-16384 to 16383)
        // This centers the waveform to prevent artifacts in the DS mixer.
		int32_t mixed = ((s_apu - DC_OFFSET) * 3) >> 1; // Apply final linear gain (1.5x factor is the sweet spot).
		// Clamp to 16-bit range (-32768 to 32767)
		clampSamples16(mixed);
        *pcmBuffer++ = (int16_t)mixed;
    }
	// Process Hardware PSG updates if enabled
    if (CurrentPulseMode == PULSE_CH_HW)
	{
        NESAPUSoundSquareHWRender();
    }
	// Sync APU logic and registers
	readAPU();
    APU4015Reg();
}

void initsound()
{
	powerOn(BIT(0));
	REG_SOUNDCNT = SOUND_ENABLE | SOUND_VOL(127);

    u16 timerVal = TIMER_NFREQ; // 32768Hz

	SCHANNEL_SOURCE(0) = (u32)&buffer[0];
	SCHANNEL_SOURCE(1) = (u32)&buffer[0];

	SCHANNEL_TIMER(0) = timerVal;
	SCHANNEL_TIMER(1) = timerVal;

	SCHANNEL_LENGTH(0) = MIXBUFSIZE;
	SCHANNEL_LENGTH(1) = MIXBUFSIZE;

	SCHANNEL_REPEAT_POINT(0) = 0;
	SCHANNEL_REPEAT_POINT(1) = 0;

	TIMER_DATA(0) = timerVal << 1;
	TIMER_CR(0) = TIMER_ENABLE;

	TIMER_DATA(1) = (u16)-MIXBUFSIZE;
	TIMER_CR(1) = TIMER_CASCADE | TIMER_IRQ_REQ | TIMER_ENABLE;
	memset(buffer, 0, sizeof(buffer));
	NesAPUSoundSquareHWStop();
}

void restartsound(int ch)
{
	chan = ch;

	SCHANNEL_CR(0) = ENBLD |
					RPEAT |
					R_VOL |
					R_PAN |
					PCM16 ;
	
	SCHANNEL_CR(1) = ENBLD |
					RPEAT |
					L_VOL |
					L_PAN |
					PCM16 ;


//TODO: Channel 9-11 reserved to mix Konami VCR7 Audio ("Lagrange Point" is the only game that uses this.)
//TODO: Channel 12-13 reserved to mix NAMCO N163 Audio (4 N163 channels per NDS channel)

	TIMER_CR(0) = TIMER_ENABLE; 
	TIMER_CR(1) = TIMER_CASCADE | TIMER_IRQ_REQ | TIMER_ENABLE;
}

void stopsound() 
{
	SCHANNEL_CR(0) = 0;
	SCHANNEL_CR(1) = 0;

	TIMER_CR(1) = 0;
	TIMER_CR(0) = 0;
	NesAPUSoundSquareHWStop();
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
	mix(chan);
	if(REG_IF & IRQ_TIMER1)
	{
		lidinterrupt();
		chan = 1;
		REG_IF = IRQ_TIMER1;
	}

}

void APUSoundWrite(Uint address, Uint value);	//from s_apu.c (skip using read handlers, just write it directly)

void fifointerrupt(u32 msg, void *none)			//This should be registered to a fifo channel.
{
	switch(msg&0xff) 
	{
		case FIFO_APU_PAUSE:
			APU_paused=1;
			memset(buffer,0,sizeof(buffer));
			NesAPUSoundSquareHWStop();
			break;
		case FIFO_UNPAUSE:
			APU_paused=0;
			break;
		case FIFO_APU_RESET:
			memset(buffer,0,sizeof(buffer));
			NesAPUSoundSquareHWStop();
			APU_paused=0;
			resetAPU();
			APU4015Reg();
			readAPU();
			break;
		case FIFO_SOUND_RESET:
			lidinterrupt();
			memset(buffer,0,sizeof(buffer));
			NesAPUSoundSquareHWStop();
			break;
		case FIFO_APU_PAL:
			SetApuPAL();
			resetAPU();
			readAPU();
			break;
		case FIFO_APU_NTSC:
			SetApuNTSC();
			resetAPU();
			readAPU();
			break;
		case FIFO_APU_SWAP:
			SetApuSwap();
			resetAPU();
			readAPU();
			break;
		case FIFO_APU_NORM:
			SetApuNormal();
			resetAPU();
			readAPU();
			break;
		case FIFO_SOUND_UPDATE:
			resetAPU();
			readAPU();
			APU4015Reg();
			break;
		case FIFO_APU_PULSE_SW:
            SetPulseModeSW();
            break;
        case FIFO_APU_PULSE_HW:
            SetPulseModeHW();
            break;	
	}
}

void readAPU()
{
	u32 msg;
	if(1) 
	{
		while((msg = fifoGetValue32(FIFO_USER_07)) != 0)
			APUSoundWrite(msg >> 8, msg & 0xFF);
		IPC_APUR = IPC_APUW;
	}
	else 
	{
		unsigned int *src = IPC_APUWRITE;
		unsigned int end = IPC_APUW;
		unsigned int start = IPC_APUR;
		while(start < end)
		{
			unsigned int val = src[start&(1024 - 1)];
			APUSoundWrite(val >> 8, val & 0xFF);
			start++;
		}
		IPC_APUR = start;
	}
}

void interrupthandler() 
{
	u32 flags=REG_IF&REG_IE;
	if(flags&IRQ_TIMER1)
		soundinterrupt();
}

void nesmain() 
{
	APUSoundInstall();
	FDSSoundInstall();
	VRC6SoundInstall();
	
	resetAPU();

	initsound();
	restartsound(1);

	fifoSetValue32Handler(FIFO_USER_08, fifointerrupt, 0);		//use the last IPC channel to comm..
	irqSet(IRQ_LID, lidinterrupt);
	irqSet(IRQ_TIMER1, soundinterrupt);
	swiWaitForVBlank();
}
