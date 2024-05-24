#include <nds.h>
#include <string.h>
#include "c_defs.h"
#include "audiosys.h"
#include "handler.h"
#include "calc_lut.h"

// Buffer for PCM samples
#define MIXBUFSIZE 128

s16 buffer [MIXBUFSIZE * 20]; // Sound Samples Buffer Size, adjust size if necessary
Uint NTMR_FREQ = (TIMER_FREQ_SHIFT(MIXFREQ,1,1)); //From GBATEK: timerval = -(33513982Hz/2)/freq

void readAPU(void);
void resetAPU(void);
void Raw_PCM_Channel(unsigned char *buffer);

// Resets thge APU emulation to avoid garbage sounds
void resetAPU() 
{
	NESReset();
	IPC_APUW = 0;
	IPC_APUR = 0;
}

static int chan = 0;

// Adjust Volume and frequency using a precalculated logarithmic table
static inline short adjust_samples(short sample, int freq_shift, int volume)
{
	if(sample >= 0)
	{
		sample = calc_table[sample << freq_shift];
	} else {
		sample = -calc_table[(-sample) << freq_shift];
	}
	return sample << volume;
}

// Adjust alignment for proper volume and frequency
static inline short adjust_vrc(short sample, int freq_shift)
{
	return sample << freq_shift;
}

void restartsound(int ch) 
{
	chan = ch;

    // Max is 0x7F, min is 0x00, defaults are 0x20 - 0x60 for mid panning, 0x40 for center
	SCHANNEL_CR(0)=SCHANNEL_ENABLE|SOUND_REPEAT |SOUND_VOL(0x5F)|SOUND_PAN(0x20)|SNDEXTCNT_FREQ_47KHZ|SOUND_FORMAT_16BIT; // VOL 0x5F | PAN 0X20 | Pulse 1
	SCHANNEL_CR(1)=SCHANNEL_ENABLE|SOUND_REPEAT |SOUND_VOL(0x5F)|SOUND_PAN(0x60)|SNDEXTCNT_FREQ_47KHZ|SOUND_FORMAT_16BIT; // VOL 0x5F | PAN 0X60 | Pulse 2
	SCHANNEL_CR(2)=SCHANNEL_ENABLE|SOUND_REPEAT |SOUND_VOL(0x7F)|SOUND_PAN(0x40)|SNDEXTCNT_FREQ_47KHZ|SOUND_FORMAT_16BIT; // VOL 0x7F | PAN 0X40 | Triangle

	// SCHANNEL_CR(3)=SCHANNEL_ENABLE|SOUND_REPEAT |SOUND_VOL(0x7F)|SOUND_PAN(0x4F)|SOUND_FORMAT_16BIT; // VOL 0X2B | PAN 0X4F | Noise
	SCHANNEL_CR(3)=SCHANNEL_ENABLE|SOUND_REPEAT |SOUND_VOL(0x5F)|SOUND_PAN(0x4F)|SNDEXTCNT_FREQ_47KHZ|SOUND_FORMAT_16BIT; // VOL 0x5F | PAN 0X4F | Noise

	SCHANNEL_CR(4)=SCHANNEL_ENABLE|SOUND_REPEAT |SOUND_VOL(0x7F)|SOUND_PAN(0x3A)|SNDEXTCNT_FREQ_47KHZ|SOUND_FORMAT_16BIT; // VOL 0x7F | PAN 0X36 | DMC
	SCHANNEL_CR(5)=SCHANNEL_ENABLE|SOUND_REPEAT |SOUND_VOL(0x7F)|SOUND_PAN(0x40)|SNDEXTCNT_FREQ_47KHZ|SOUND_FORMAT_16BIT; // VOL 0x7F | PAN 0X40 | FDS

	// VCR6 VCR7 are 16BIT PCM, no conversion needed
	SCHANNEL_CR(6)=SCHANNEL_ENABLE|SOUND_REPEAT |SOUND_VOL(0x5F)|SOUND_PAN(0x2A)|SNDEXTCNT_FREQ_47KHZ|SOUND_FORMAT_16BIT; // VOL 0x5F | PAN 0X2A | VRC6 Saw 1
	SCHANNEL_CR(7)=SCHANNEL_ENABLE|SOUND_REPEAT |SOUND_VOL(0x5F)|SOUND_PAN(0x50)|SNDEXTCNT_FREQ_47KHZ|SOUND_FORMAT_16BIT; // VOL 0x5F | PAN 0X56 | VRC6 Saw 2
	SCHANNEL_CR(8)=SCHANNEL_ENABLE|SOUND_REPEAT |SOUND_VOL(0x5F)|SOUND_PAN(0x60)|SNDEXTCNT_FREQ_47KHZ|SOUND_FORMAT_16BIT; // VOL 0x5F | PAN 0X40 | VRC6 Saw 3

	SCHANNEL_CR(10)=SCHANNEL_ENABLE|SOUND_REPEAT |SOUND_VOL(0x7F)|SOUND_PAN(0x40)|SNDEXTCNT_FREQ_47KHZ|SOUND_FORMAT_16BIT; // VOL 0X7F | PAN 0X40 | PCM

	TIMER0_CR = TIMER_ENABLE; 
	TIMER1_CR = TIMER_CASCADE | TIMER_IRQ_REQ | TIMER_ENABLE;
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
	SCHANNEL_CR(10) = 0;
	TIMER0_CR = 0;
	TIMER1_CR = 0;
}

int pcmpos = 0;
int APU_paused = 0;

int32 NESAPUSoundSquareRender1();
int32 NESAPUSoundSquareRender2();

int32 NESAPUSoundTriangleRender1();
int32 NESAPUSoundNoiseRender1();
int32 NESAPUSoundDpcmRender1();

int32 FDSSoundRender1();
int32 FDSSoundRender2();
int32 FDSSoundRender3();

int32 VRC6SoundRender1();
int32 VRC6SoundRender2();
int32 VRC6SoundRender3();

void VRC6SoundInstall();
void readAPU();

void mix(int chan)
{
    int mapper = IPC_MAPPER;
    if (!APU_paused) 
	{
        int i;
        s16 *pcmBuffer = &buffer[chan*MIXBUFSIZE]; // Pointer to PCM buffer

        for (i = 0; i < MIXBUFSIZE; i++)
		{
            // static int32_t preval = 0;
            *pcmBuffer++ = adjust_samples(NESAPUSoundSquareRender1(), 6, 4);
            // output  = (preval + output) >> 1; // Separated into their own filters
			// *pcmBuffer++  = output;
            // preval = output;
        }

		pcmBuffer+=MIXBUFSIZE;
  		for (i = 0; i < MIXBUFSIZE; i++) 
 		{
            // static int32_t preval = 0;
            *pcmBuffer++ = adjust_samples(NESAPUSoundSquareRender2(), 6, 4);
			// output  = (preval + output) >> 1; // Separated into their own filters
			// *pcmBuffer++  = output;
            // preval = output;
        }

		pcmBuffer+=MIXBUFSIZE;
        for (i = 0; i < MIXBUFSIZE; i++) 
		{
            // static int32_t preval = 0;
            *pcmBuffer++ = adjust_samples(NESAPUSoundTriangleRender1(), 7, 4);
            // output  = (preval + output) >> 1;
			// *pcmBuffer++  = output;
            // preval = output;
        }

		pcmBuffer+=MIXBUFSIZE;
        for (i = 0; i < MIXBUFSIZE; i++) 
		{
            // static int32_t preval = 0;
            *pcmBuffer++ = adjust_samples(NESAPUSoundNoiseRender1(), 6, 4);
			// output  = (preval + output) >> 1; // Separated into their own filters
			// *pcmBuffer++  = output;
            // preval = output;
        }

		pcmBuffer+=MIXBUFSIZE;
        for (i = 0; i < MIXBUFSIZE; i++) 
		{
            // static int32_t preval = 0;
            *pcmBuffer++ = adjust_samples(NESAPUSoundDpcmRender1(), 4, 5);
            // output  = (preval + output) >> 1; // Separated into their own filters
			// *pcmBuffer++  = output;
            // preval = output;
        }

		pcmBuffer+=MIXBUFSIZE;
        if (mapper == 20 || mapper == 256)
		{
            for (i = 0; i < MIXBUFSIZE; i++) 
			{
                // static int32_t preval = 0;
                *pcmBuffer++ = adjust_samples(FDSSoundRender3(), 0, 4);
           		// output  = (preval + output) >> 1; // Separated into their own filters
				// *pcmBuffer++  = output;
        	    // preval = output;
            }
		} 
		    else
			{
		    pcmBuffer+=MIXBUFSIZE;
			}

		pcmBuffer+=MIXBUFSIZE;	
        if (mapper == 24 || mapper == 26 || mapper == 256) 
		{
            for (i = 0; i < MIXBUFSIZE; i++)
			{
                // static int32_t preval = 0;
                int32_t output = adjust_vrc(VRC6SoundRender1(), 11);
            	// output  = (preval + output) >> 1; // Separated into their own filters
				*pcmBuffer++  = output << 1;
            	// preval = output;
            }

			pcmBuffer+=MIXBUFSIZE;
            for (i = 0; i < MIXBUFSIZE; i++) 
			{
                // static int32_t preval = 0;
                int32_t output = adjust_vrc(VRC6SoundRender2(), 11);
            	// output  = (preval + output) >> 1; // Separated into their own filters
				*pcmBuffer++  = output << 1; // Adjust volume for VRC Samples
            	// preval = output;
            }

			pcmBuffer+=MIXBUFSIZE;
            for (i = 0; i < MIXBUFSIZE; i++) 
			{
                // static int32_t preval = 0;
                int32_t output = adjust_vrc(VRC6SoundRender3(), 10);
              	// output  = (preval + output) >> 1; // Separated into their own filters
				*pcmBuffer++  = output << 1; // Adjust volume for VRC Samples
            	//preval = output;
            }
        }
		// Mix raw pcm
		Raw_PCM_Channel((u8 *)&buffer[chan*(MIXBUFSIZE >> 1) + MIXBUFSIZE * 18]);
    }
    readAPU();
    APU4015Reg(); // to refresh reg4015.
}

void initsound() 
{ 		
	int i;
	powerOn(POWER_SOUND); 
	REG_SOUNDCNT = SOUND_ENABLE | SOUND_VOL(0x52);
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
	SCHANNEL_SOURCE(10) = (u32)&buffer[18*MIXBUFSIZE];

	SCHANNEL_TIMER(0) = NTMR_FREQ;
	SCHANNEL_TIMER(1) = NTMR_FREQ;
	SCHANNEL_TIMER(2) = NTMR_FREQ;
	SCHANNEL_TIMER(3) = NTMR_FREQ;
	SCHANNEL_TIMER(4) = NTMR_FREQ;
	SCHANNEL_TIMER(5) = NTMR_FREQ;
	SCHANNEL_TIMER(6) = NTMR_FREQ;
	SCHANNEL_TIMER(7) = NTMR_FREQ;
	SCHANNEL_TIMER(8) = NTMR_FREQ;
	SCHANNEL_TIMER(10) = NTMR_FREQ << 1;

	SCHANNEL_LENGTH(0) = MIXBUFSIZE;
	SCHANNEL_LENGTH(1) = MIXBUFSIZE;
	SCHANNEL_LENGTH(2) = MIXBUFSIZE;
	SCHANNEL_LENGTH(3) = MIXBUFSIZE;
	SCHANNEL_LENGTH(4) = MIXBUFSIZE;
	SCHANNEL_LENGTH(5) = MIXBUFSIZE;
	SCHANNEL_LENGTH(6) = MIXBUFSIZE;
	SCHANNEL_LENGTH(7) = MIXBUFSIZE;
	SCHANNEL_LENGTH(8) = MIXBUFSIZE;
	SCHANNEL_LENGTH(10) = MIXBUFSIZE >> 1;

	SCHANNEL_REPEAT_POINT(0) = 0;
	SCHANNEL_REPEAT_POINT(1) = 0;
	SCHANNEL_REPEAT_POINT(2) = 0;
	SCHANNEL_REPEAT_POINT(3) = 0;
	SCHANNEL_REPEAT_POINT(4) = 0;
	SCHANNEL_REPEAT_POINT(5) = 0;
	SCHANNEL_REPEAT_POINT(6) = 0;
	SCHANNEL_REPEAT_POINT(1) = 0;
	SCHANNEL_REPEAT_POINT(8) = 0;
	SCHANNEL_REPEAT_POINT(10) = 0;

	TIMER0_DATA = NTMR_FREQ << 1;
	TIMER1_DATA = 0x10000 - MIXBUFSIZE;
	memset(buffer, 0, sizeof(buffer));

	memset(IPC_PCMDATA, 0, 512);
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

static unsigned char pcm_out = 0x3F;
int pcm_line = 120;
int pcmprevol = 0x3F;

// Only works well with 24064 sound frequency, needs review
void Raw_PCM_Channel(unsigned char *buffer)
{
	unsigned char *in = IPC_PCMDATA;
	int i;
	int count = 0;
	int line = 0;
	unsigned char *outp = buffer;

	pcm_line = REG_VCOUNT;

	if(1) 
	{
		for(i = 0; i < MIXBUFSIZE; i++) 
		{
			if(in[pcm_line] & 0x80) 
			{
				pcm_out = in[pcm_line] & 0x7F;
				in[pcm_line] = 0;
				count++;
			}
			*buffer++ = (pcm_out + pcmprevol - 0x80);
			pcmprevol = pcm_out;
			line += 100;
			if(line >= 152) 
			{
				line -= 152;
				pcm_line++;
				if(pcm_line > 262) 
				{
					pcm_line = 0;
				}
			}
		}
	}
	//not a playable raw pcm.
	if(count < 10) 
	{
		for(i = 0; i < MIXBUFSIZE; i++) 
		{
			*outp++ = 0;
			pcmprevol = 0x3F;
			pcm_out = 0x3F;
		}
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
			break;
		case FIFO_SOUND_RESET:
			lidinterrupt();
			break;
	}
}

void resetAPU() 
{
	NESReset();
	IPC_APUW = 0;
	IPC_APUR = 0;
}

void readAPU()
{
	u32 msg;
	if(1) {
		while((msg = fifoGetValue32(FIFO_USER_07)) != 0)
			APUSoundWrite(msg >> 8, msg&0xFF);
		IPC_APUR = IPC_APUW;
	}
	else {
		unsigned int *src = IPC_APUWRITE;
		unsigned int end = IPC_APUW;
		unsigned int start = IPC_APUR;
		while(start < end) {
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
	NESAudioFrequencySet(MIXFREQ);
	NESTerminate();
	NESHandlerInitialize();
	NESAudioHandlerInitialize();
	APUSoundInstall();
	FDSSoundInstall();
	VRC6SoundInstall();
	
	resetAPU();
	NESVolume(0);

	
	swiWaitForVBlank();
	initsound();
	restartsound(1);

	fifoSetValue32Handler(FIFO_USER_08, fifointerrupt, 0);		//use the last IPC channel to comm..
	irqSet(IRQ_TIMER1, soundinterrupt);
	//irqSet(IRQ_LID, lidinterrupt);
}
