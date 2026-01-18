#include <nds.h>
#include <nds/arm7/audio.h>
#include <string.h>
#include "c_defs.h"
#include "SoundIPC.h"
#include "audiosys.h"
#include "handler.h"
#include "calc_lut.h"
#include "s_defs.h"
#include "s_vrc6.h"

s16 buffer [MIXBUFSIZE * 20]; // Sound Samples Buffer Size, adjust size if necessary

// Set Flag for the APU settings to match PAL Sound Frequency
enum ApuRegion ApuCurrentRegion = NTSC;

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

// SWAP CYCLES
enum ApuStatus ApuCurrentStatus = Normal;

void SetApuSwap()
{
	ApuCurrentStatus = Reverse;
}

void SetApuNormal()
{
	ApuCurrentStatus = Normal;
}

enum ApuStatus getApuCurrentStatus()
{
	return ApuCurrentStatus;
}

void readAPU(void);
void resetAPU(void);

// Resets thge APU emulation to avoid garbage sounds
void resetAPU() 
{
	NESReset();
	IPC_APUW = 0;
	IPC_APUR = 0;
}

// Adjust Volume and frequency using a precalculated logarithmic table
static inline short adjust_samples(short sample, int freq_shift, int volume)
{

   	int index = sample >= 0 ? sample << freq_shift : (-sample) << freq_shift;
    if (index >= 1024) index = 1023; // Limit max input size
    
    sample = sample >= 0 ? calc_table[index] : -calc_table[index];
    return sample << volume;
}

static int chan = 0;
int ENBLD = SCHANNEL_ENABLE;
int RPEAT = SOUND_REPEAT;
int PCM_8 = SOUND_FORMAT_8BIT;
int PCM16 = SOUND_FORMAT_16BIT;
int ADPCM = SOUND_FORMAT_ADPCM;

int P1_VL = SOUND_VOL(0x15); // VOL 0x5F
int P1_PN = SOUND_PAN(0x42); // PAN 0X20

// Pulse 2
int P2_VL = SOUND_VOL(0x15); // VOL 0x5F
int P2_PN = SOUND_PAN(0x3D); // PAN 0X60

// Triangle
int TR_VL = SOUND_VOL(0x60); // VOL 0x7F
int TR_PN = SOUND_PAN(0x40); // PAN 0X20

// Noise
int NS_VL = SOUND_VOL(0x7F); // VOL 0x7A
int NS_PN = SOUND_PAN(0x40); // PAN 0X45

// DMC
int DM_VL = SOUND_VOL(0x7F); // VOL 0x6F
int DM_PN = SOUND_PAN(0x40); // PAN 0x40

// FDS
int F1_VL = SOUND_VOL(0x5F); // VOL 0x7F
int F1_PN = SOUND_PAN(0x40); // PAN 0X40

// VRC6 Square 1
int V1_VL = SOUND_VOL(0x20); // VOL 0x3C
int V1_PN = SOUND_PAN(0x45); // PAN 0x54

// VRC6 Square 2
int V2_VL = SOUND_VOL(0x20); // VOL 0x3C
int V2_PN = SOUND_PAN(0x3B); // PAN 0x54

// VRC6 Saw
int V3_VL = SOUND_VOL(0x20); // VOL 0x3C
int V3_PN = SOUND_PAN(0x3F); // PAN 0x54

void restartsound(int ch)
{
	chan = ch;

	SCHANNEL_CR(0) = ENBLD |
					RPEAT |
					//Pulse1_volume()|
					P1_VL |
					P1_PN |
					PCM16 ;
	
	SCHANNEL_CR(1) = ENBLD |
					RPEAT |
					P2_VL |
					P2_PN |
					PCM16 ;

	SCHANNEL_CR(2) = ENBLD |
					RPEAT |
					TR_VL |
					TR_PN |
					PCM16 ;

	SCHANNEL_CR(3) = ENBLD |
					RPEAT |
					NS_VL |
					NS_PN |
					PCM16 ;

	SCHANNEL_CR(4) = ENBLD |
					RPEAT |
					DM_VL |
					DM_PN |
					PCM16 ;
	
	SCHANNEL_CR(5) = ENBLD |
					RPEAT |
					F1_VL |
					F1_PN |
					PCM16 ;

	SCHANNEL_CR(6) = ENBLD |
					RPEAT |
					V1_VL |
					V1_PN |
					PCM16 ;
	
	SCHANNEL_CR(7) = ENBLD |
					RPEAT |
					V2_VL |
					V2_PN |
					PCM16 ;
	
	SCHANNEL_CR(8) = ENBLD |
					RPEAT |
					V3_VL |
					V3_PN |
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
	SCHANNEL_CR(2) = 0;
	SCHANNEL_CR(3) = 0;
	SCHANNEL_CR(4) = 0;
	SCHANNEL_CR(5) = 0;
	SCHANNEL_CR(6) = 0;
	SCHANNEL_CR(7) = 0;
	SCHANNEL_CR(8) = 0;
	TIMER_CR(1) = 0;
	TIMER_CR(0) = 0;
}

int pcmpos = 0;
int APU_paused = 0;

// Mixer handler TODO: Implement cases for custom sound filters.
void __fastcall mix(int chan)
{
    int mapper = IPC_MAPPER;

    if (!APU_paused) 
	{
        int i;
        s16 *pcmBuffer = &buffer[chan*MIXBUFSIZE];

        for (i = 0; i < MIXBUFSIZE; i++)
		{			
			int32 output = adjust_samples(NESAPUSoundSquareRender1(), 6, 4);
			*pcmBuffer++ = output;
        }

		pcmBuffer+=MIXBUFSIZE;
  		for (i = 0; i < MIXBUFSIZE; i++)
 		{
            int32 output = adjust_samples(NESAPUSoundSquareRender2(), 6, 4);
			*pcmBuffer++ = output;
        }

		pcmBuffer+=MIXBUFSIZE;
        for (i = 0; i < MIXBUFSIZE; i++)
		{
            int32 output = NESAPUSoundTriangleRender1() << 9;
			*pcmBuffer++ = output;
        }

		pcmBuffer+=MIXBUFSIZE;
        for (i = 0; i < MIXBUFSIZE; i++) 
		{
            int32 output = NESAPUSoundNoiseRender1() << 9;
			*pcmBuffer++ = output;
        }

		pcmBuffer+=MIXBUFSIZE;
        for (i = 0; i < MIXBUFSIZE; i++) 
		{
            int32 output = NESAPUSoundDpcmRender1() << 9;
			*pcmBuffer++ = output;
        }

		pcmBuffer+=MIXBUFSIZE;
        if (mapper == 20 || mapper == 256)
		{
            for (i = 0; i < MIXBUFSIZE; i++) 
			{
                int32 output = adjust_samples(FDSSoundRender(), 0, 4);
				*pcmBuffer++ = output;
            }
		} 
		    else
			{
		    pcmBuffer+=MIXBUFSIZE;
			}

		//pcmBuffer+=MIXBUFSIZE;	
        if (mapper == 24 || mapper == 26)
		{
			pcmBuffer+=MIXBUFSIZE;
            for (i = 0; i < MIXBUFSIZE; i++)
			{
				int32 output = VRC6SoundRenderSquare1() << 11;
				*pcmBuffer++ = output;
            }

			pcmBuffer+=MIXBUFSIZE;
            for (i = 0; i < MIXBUFSIZE; i++) 
			{
				int32 output = VRC6SoundRenderSquare2() << 11;
				*pcmBuffer++ = output;
            }

			pcmBuffer+=MIXBUFSIZE;
            for (i = 0; i < MIXBUFSIZE; i++)
			{
				int32 output = VRC6SoundRenderSaw() << 11;
				*pcmBuffer++ = output;
            }
        }
    }
    readAPU();
    APU4015Reg(); // to refresh reg4015.
}

void initsound()
{ 		
	int i;
	powerOn(BIT(0));
	REG_SOUNDCNT = SOUND_ENABLE | SOUND_VOL(0x7F);
	for(i = 0; i < 16; i++) 
	{
		SCHANNEL_CR(i) = 0;
	}

	SCHANNEL_SOURCE(0) = (u32)&buffer[0];
	SCHANNEL_SOURCE(1) = (u32)&buffer[2*MIXBUFSIZE];
	SCHANNEL_SOURCE(2) = (u32)&buffer[4*MIXBUFSIZE];
	SCHANNEL_SOURCE(3) = (u32)&buffer[6*MIXBUFSIZE];
	SCHANNEL_SOURCE(4) = (u32)&buffer[8*MIXBUFSIZE];
	SCHANNEL_SOURCE(5) = (u32)&buffer[10*MIXBUFSIZE];
	SCHANNEL_SOURCE(6) = (u32)&buffer[12*MIXBUFSIZE];
	SCHANNEL_SOURCE(7) = (u32)&buffer[14*MIXBUFSIZE];
	SCHANNEL_SOURCE(8) = (u32)&buffer[16*MIXBUFSIZE];

	SCHANNEL_TIMER(0) = TIMER_NFREQ;
	SCHANNEL_TIMER(1) = TIMER_NFREQ;
	SCHANNEL_TIMER(2) = TIMER_NFREQ;
	SCHANNEL_TIMER(3) = TIMER_NFREQ;
	SCHANNEL_TIMER(4) = TIMER_NFREQ;
	SCHANNEL_TIMER(5) = TIMER_NFREQ;
	SCHANNEL_TIMER(6) = TIMER_NFREQ;
	SCHANNEL_TIMER(7) = TIMER_NFREQ;
	SCHANNEL_TIMER(8) = TIMER_NFREQ;

	SCHANNEL_LENGTH(0) = MIXBUFSIZE;
	SCHANNEL_LENGTH(1) = MIXBUFSIZE;
	SCHANNEL_LENGTH(2) = MIXBUFSIZE;
	SCHANNEL_LENGTH(3) = MIXBUFSIZE;
	SCHANNEL_LENGTH(4) = MIXBUFSIZE;
	SCHANNEL_LENGTH(5) = MIXBUFSIZE;
	SCHANNEL_LENGTH(6) = MIXBUFSIZE;
	SCHANNEL_LENGTH(7) = MIXBUFSIZE;
	SCHANNEL_LENGTH(8) = MIXBUFSIZE;

	SCHANNEL_REPEAT_POINT(0) = 0;
	SCHANNEL_REPEAT_POINT(1) = 0;
	SCHANNEL_REPEAT_POINT(2) = 0;
	SCHANNEL_REPEAT_POINT(3) = 0;
	SCHANNEL_REPEAT_POINT(4) = 0;
	SCHANNEL_REPEAT_POINT(5) = 0;
	SCHANNEL_REPEAT_POINT(6) = 0;
	SCHANNEL_REPEAT_POINT(7) = 0;
	SCHANNEL_REPEAT_POINT(8) = 0;

	TIMER_DATA(0) = TIMER_NFREQ << 1;
	TIMER_DATA(1) = 0x10000 - MIXBUFSIZE;
	memset(buffer, 0, sizeof(buffer));
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
			break;
		case FIFO_UNPAUSE:
			APU_paused=0;
			break;
		case FIFO_APU_RESET:
			memset(buffer,0,sizeof(buffer));
			APU_paused=0;
			resetAPU();
			APU4015Reg();
			readAPU();
			break;
		case FIFO_SOUND_RESET:
			lidinterrupt();
			memset(buffer,0,sizeof(buffer));
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
	//NESAudioFrequencySet(MIXFREQ);
	//NESTerminate();
	//NESHandlerInitialize();
	//NESAudioHandlerInitialize();
	
	// Change func name to "DPCMSoundInstall();"
	APUSoundInstall();
	FDSSoundInstall();
	VRC6SoundInstall();
	
	resetAPU();

	swiWaitForVBlank();
	initsound();
	restartsound(1);

	fifoSetValue32Handler(FIFO_USER_08, fifointerrupt, 0);		//use the last IPC channel to comm..
	irqSet(IRQ_TIMER1, soundinterrupt);
	irqSet(IRQ_LID, lidinterrupt);
}
