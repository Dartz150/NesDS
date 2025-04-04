#include <nds.h>
#include <nds/arm7/audio.h>
#include <string.h>
#include "c_defs.h"
#include "SoundIPC.h"
#include "audiosys.h"
#include "handler.h"
#include "calc_lut.h"
#include "mixer.h"
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

// Adjust alignment for proper volume and frequency
// static inline short adjust_vrc(short sample, int freq_shift)
// {
// 	return sample << freq_shift;
// }


// NES APU Reg $4011, RAW PCM
int32 Raw_PCM_Channel(u8 *buffer)
{
static unsigned char pcm_out = 0x3F;
// This needs to be changed along with the Frequency
// 120 for freq 24064Hz (697 timer freq) 
// and 163 for DS frequency (32768Hz, 511 timer freq).
// TODO: Add autoajust ratio formula, line/freq ratio yet unknown.
int pcm_line = 163;
int pcmprevol = 0x3F;

	u8 *in = IPC_PCMDATA;
	int i;
	int count = 0;
	int line = 0;
	u8 *outp = buffer;

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

			// This needs to be changed along with the Frequency
			// 152 for freq 24064Hz (697 timer freq) 
			// and 207 for DS frequency (32768Hz, 511 timer freq).
			// TODO: Add autoajust ratio formula, line/freq ratio yet unknown.
			if(line >= 207)
			{
				line -= 207;
				pcm_line++;
				if(pcm_line > 262)
				{
					pcm_line = 0;
				}
			}
		}
	}
	//not a playable raw pcm.
	if(count < 13) 
	{
		for(i = 0; i < MIXBUFSIZE; i++) 
		{
			*outp++ = 0;
			pcmprevol = 0x3F;
			pcm_out = 0x3F;
		}
	}
}

//----------------------------------------------//
//                                              //
//********SOUND MIXER CHANNELS PARAMETERS*******//
//                                              //
//----------------------------------------------//

static int chan = 0;
int ENBLD = SCHANNEL_ENABLE;
int RPEAT = SOUND_REPEAT;
int PCM_8 = SOUND_FORMAT_8BIT;
int PCM16 = SOUND_FORMAT_16BIT;
int ADPCM = SOUND_FORMAT_ADPCM;

// Ch Volume and Pan Control  // Default Values 0Min ~ 127Max (0x0 ~ 0x7F)
// Max is 0x7F, min is 0x00, defaults are 0x20 - 0x60 for mid panning, 0x40 for center

// Pulse 1
// int def_volume = 0x3F; // Dummy Value

// int Pulse1_volume()
// {
// 	int init_val = SOUND_VOL(0x00);
// 	int P1_VL = init_val + def_volume; // VOL 0x5F
// 	return P1_VL;
// }

int P1_VL = SOUND_VOL(0x40); // VOL 0x5F
int P1_PN = SOUND_PAN(0x20); // PAN 0X20

// Pulse 2
int P2_VL = SOUND_VOL(0x45); // VOL 0x5F
int P2_PN = SOUND_PAN(0x60); // PAN 0X60

// Triangle
int TR_VL = SOUND_VOL(0x45); // VOL 0x7F
int TR_PN = SOUND_PAN(0x40); // PAN 0X20

// Noise
int NS_VL = SOUND_VOL(0x74); // VOL 0x7A
int NS_PN = SOUND_PAN(0x45); // PAN 0X45

// DMC
int DM_VL = SOUND_VOL(0x7F); // VOL 0x6F
int DM_PN = SOUND_PAN(0x40); // PAN 0x40

// FDS
int F1_VL = SOUND_VOL(0x5F); // VOL 0x7F
int F1_PN = SOUND_PAN(0x40); // PAN 0X40

// VRC6 Square 1
int V1_VL = SOUND_VOL(0x7A); // VOL 0x3C
int V1_PN = SOUND_PAN(0x54); // PAN 0x54

// VRC6 Square 2
int V2_VL = SOUND_VOL(0x7A); // VOL 0x3C
int V2_PN = SOUND_PAN(0x2C); // PAN 0x54

// VRC6 Saw
int V3_VL = SOUND_VOL(0x54); // VOL 0x3C
int V3_PN = SOUND_PAN(0x40); // PAN 0x54

// Delta PCM Channel
int RP_VL = SOUND_VOL(0x7F); // VOL 0x7F
int RP_PN = SOUND_PAN(0x40); // PAN 0x40

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

// Delta PCM // TODO: Move to channel 14
	SCHANNEL_CR(9) = ENBLD |
					RPEAT |
					RP_VL |
					RP_PN |
					PCM16 ;

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
	SCHANNEL_CR(9) = 0;
	TIMER_CR(1) = 0;
	TIMER_CR(0) = 0;
}

int pcmpos = 0;
int APU_paused = 0;

//  //Set Default Filter Type
// enum AudioFilterType CurrentFilterType = NES_AUDIO_FILTER_NONE;


// //Get New Filter Type from ARM9
// void setAudioFilter()
// {
// 	switch (FIFO_AUDIO_FILTER)
// 	{
// 	case FIFO_AUDIO_FILTER << 0:
// 		CurrentFilterType = NES_AUDIO_FILTER_NONE;
// 		break;
// 	case FIFO_AUDIO_FILTER << 1:
// 		CurrentFilterType = NES_AUDIO_FILTER_CRISP;
// 		break;
// 	case FIFO_AUDIO_FILTER << 2:
// 	 	CurrentFilterType = NES_AUDIO_FILTER_OLDTV;
// 		break;
// 	case FIFO_AUDIO_FILTER << 3:
// 		CurrentFilterType = NES_AUDIO_FILTER_LOWPASS;
// 		break;
// 	case FIFO_AUDIO_FILTER << 4:
// 		CurrentFilterType = NES_AUDIO_FILTER_HIGHPASS;
// 		break;
// 	case FIFO_AUDIO_FILTER << 5:
// 		CurrentFilterType = NES_AUDIO_FILTER_WEIGHTED;
// 		break;
// 	}
// }
// // Filter Type Get from Settings
// enum AudioFilterType getAudioFilterType()
// {
// 	return CurrentFilterType;
// }

// // //Audio Filters
// short int lowpass(signed short input)
// {
// short int output = 0;
// static short int accum = 0;
// 	switch (CurrentFilterType)
// 	{
// 	// Default No Filter
// 	case NES_AUDIO_FILTER_NONE:
// 		return;
// 		break;
// 	//Modern RF TV Filter
// 	case NES_AUDIO_FILTER_CRISP:
// 		return;
// 		break;
// 	//Old TV Filter
// 	case NES_AUDIO_FILTER_OLDTV:
// 		output = ((input >> 1) + (accum * 6)) >> 3;
// 		accum = output;
// 		return output;	
// 		break;
// 	// Famicom/NES Filter
// 	case NES_AUDIO_FILTER_LOWPASS:
// 		output = (input + (accum * 7)) >> 3;
// 		accum = output;
// 		return output;
// 		break;
// 	case NES_AUDIO_FILTER_HIGHPASS:
// 		return;
// 		break;
// 	case NES_AUDIO_FILTER_WEIGHTED:
// 		return;
// 		break;
// 	}
// }

// Mixer handler TODO: Implement cases for custom sound filters.
void __fastcall mix(int chan)
{
    int mapper = IPC_MAPPER;

	int32 (*VRC6SoundRender1)();
    int32 (*VRC6SoundRender2)();
    int32 (*VRC6SoundRender3)();

    if (mapper == 24) {
        VRC6SoundRender1 = VRC6SoundRender1_24;
        VRC6SoundRender2 = VRC6SoundRender2_24;
        VRC6SoundRender3 = VRC6SoundRender3_24;
    } else if (mapper == 26) {
        VRC6SoundRender1 = VRC6SoundRender1_26;
        VRC6SoundRender2 = VRC6SoundRender2_26;
        VRC6SoundRender3 = VRC6SoundRender3_26;
    } else {
        VRC6SoundRender1 = NULL;
        VRC6SoundRender2 = NULL;
        VRC6SoundRender3 = NULL;
    }

    if (!APU_paused) 
	{
        int i;
        s16 *pcmBuffer = &buffer[chan*MIXBUFSIZE]; // Pointer to PCM buffer

        for (i = 0; i < MIXBUFSIZE; i++)
		{			
			int32 output = adjust_samples(NESAPUSoundSquareRender1(), 6, 4);
			//short int output = lowpass(input);
			*pcmBuffer++ = output;
        }

		pcmBuffer+=MIXBUFSIZE;
  		for (i = 0; i < MIXBUFSIZE; i++)
 		{
            int32 output = adjust_samples(NESAPUSoundSquareRender2(), 6, 4);
			//short int output = lowpass(input);
			*pcmBuffer++ = output;
        }

		pcmBuffer+=MIXBUFSIZE;
        for (i = 0; i < MIXBUFSIZE; i++)
		{
            int32 output = adjust_samples(NESAPUSoundTriangleRender1(), 7, 5);
			//short int output = lowpass(input);
			*pcmBuffer++ = output;
        }

		pcmBuffer+=MIXBUFSIZE;
        for (i = 0; i < MIXBUFSIZE; i++) 
		{
            int32 output = adjust_samples(NESAPUSoundNoiseRender1(), 6, 3);
			//short int output = lowpass(input);
			*pcmBuffer++ = output;
        }

		pcmBuffer+=MIXBUFSIZE;
        for (i = 0; i < MIXBUFSIZE; i++) 
		{
            int32 output = adjust_samples(NESAPUSoundDpcmRender1(), 4, 5);
			//short int output = lowpass(input);
			*pcmBuffer++ = output;
        }

		pcmBuffer+=MIXBUFSIZE;
        if (mapper == 20 || mapper == 256)
		{
            for (i = 0; i < MIXBUFSIZE; i++) 
			{
                int32 output = adjust_samples(FDSSoundRender(), 0, 4);
				//short int output = lowpass(input);
				*pcmBuffer++ = output;
            }
		} 
		    else
			{
		    pcmBuffer+=MIXBUFSIZE;
			}

		//pcmBuffer+=MIXBUFSIZE;	
        if (VRC6SoundRender1 && VRC6SoundRender2 && VRC6SoundRender3)
		{
			pcmBuffer+=MIXBUFSIZE;
            for (i = 0; i < MIXBUFSIZE; i++)
			{
				int32 output = VRC6SoundRender1() << 11;
				//short int output = lowpass(input);
				*pcmBuffer++ = output;
            }

			pcmBuffer+=MIXBUFSIZE;
            for (i = 0; i < MIXBUFSIZE; i++) 
			{
				int32 output = VRC6SoundRender2() << 11;
				//short int output = lowpass(input);
				*pcmBuffer++ = output;
            }

			pcmBuffer+=MIXBUFSIZE;
            for (i = 0; i < MIXBUFSIZE; i++)
			{
				int32 output = VRC6SoundRender3() << 11;
				//short int output = lowpass(input);
				*pcmBuffer++ = output;
            }
        }	
		Raw_PCM_Channel((u8 *)&buffer[chan * (MIXBUFSIZE / 2) + MIXBUFSIZE * 18]);
    }
    readAPU();
    APU4015Reg(); // to refresh reg4015.
}

void initsound()
{ 		
	int i;
	powerOn(BIT(0));
	REG_SOUNDCNT = SOUND_ENABLE | SOUND_VOL(0x70);
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
	SCHANNEL_SOURCE(9) = (u32)&buffer[18*MIXBUFSIZE];

	SCHANNEL_TIMER(0) = TIMER_NFREQ;
	SCHANNEL_TIMER(1) = TIMER_NFREQ;
	SCHANNEL_TIMER(2) = TIMER_NFREQ;
	SCHANNEL_TIMER(3) = TIMER_NFREQ;
	SCHANNEL_TIMER(4) = TIMER_NFREQ;
	SCHANNEL_TIMER(5) = TIMER_NFREQ;
	SCHANNEL_TIMER(6) = TIMER_NFREQ;
	SCHANNEL_TIMER(7) = TIMER_NFREQ;
	SCHANNEL_TIMER(8) = TIMER_NFREQ;
	SCHANNEL_TIMER(9) = TIMER_NFREQ << 1;

	SCHANNEL_LENGTH(0) = MIXBUFSIZE;
	SCHANNEL_LENGTH(1) = MIXBUFSIZE;
	SCHANNEL_LENGTH(2) = MIXBUFSIZE;
	SCHANNEL_LENGTH(3) = MIXBUFSIZE;
	SCHANNEL_LENGTH(4) = MIXBUFSIZE;
	SCHANNEL_LENGTH(5) = MIXBUFSIZE;
	SCHANNEL_LENGTH(6) = MIXBUFSIZE;
	SCHANNEL_LENGTH(7) = MIXBUFSIZE;
	SCHANNEL_LENGTH(8) = MIXBUFSIZE;
	SCHANNEL_LENGTH(9) = MIXBUFSIZE / 2;

	SCHANNEL_REPEAT_POINT(0) = 0;
	SCHANNEL_REPEAT_POINT(1) = 0;
	SCHANNEL_REPEAT_POINT(2) = 0;
	SCHANNEL_REPEAT_POINT(3) = 0;
	SCHANNEL_REPEAT_POINT(4) = 0;
	SCHANNEL_REPEAT_POINT(5) = 0;
	SCHANNEL_REPEAT_POINT(6) = 0;
	SCHANNEL_REPEAT_POINT(1) = 0;
	SCHANNEL_REPEAT_POINT(8) = 0;
	SCHANNEL_REPEAT_POINT(9) = 0;

	TIMER_DATA(0) = TIMER_NFREQ << 1;
	TIMER_DATA(1) = 0x10000 - MIXBUFSIZE;
	memset(buffer, 0, sizeof(buffer));

	memset(IPC_PCMDATA, 0, 128);
}  

// // Configure Left Channel Capture
// void startSoundCapture0(void *buffer, u16 length) 
// {
//     REG_SNDCAP0DAD = (u32)buffer; // Dirección de destino de la captura
//     REG_SNDCAP0LEN = length;      // Longitud del búfer de captura
//     REG_SNDCAP0CNT = (0 << 1) |   // Capturar del mezclador izquierdo
//                      (0 << 2) |   // Captura en bucle
//                      (0 << 3) |   // Formato PCM16
//                      (1 << 7);    // Iniciar la captura
// }

// // Configure Right Channel Capture
// void startSoundCapture1(void *buffer, u16 length)
// {
//     REG_SNDCAP1DAD = (u32)buffer; // Dirección de destino de la captura
//     REG_SNDCAP1LEN = length;      // Longitud del búfer de captura
//     REG_SNDCAP1CNT = (1 << 1) |   // Capturar del mezclador derecho
//                      (0 << 2) |   // Captura en bucle
//                      (0 << 3) |   // Formato PCM16
//                      (1 << 7);    // Iniciar la captura
// }
// u16 capture_buffer_lenght = sizeof(buffer);
// u16 *pcmBufferCapture = sizeof(buffer)+1;

// Capture Audio for reverb and pseudo-surround effect
// void startSoundCapture0(void *pcmBufferCapture, u16 capture_buffer_lenght);
// void startSoundCapture1(void *pcmBufferCapture, u16 capture_buffer_lenght);

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
	VRC6SoundInstall_24();
	VRC6SoundInstall_26();
	
	resetAPU();
	NESVolume(0);

	swiWaitForVBlank();
	initsound();
	restartsound(1);

	fifoSetValue32Handler(FIFO_USER_08, fifointerrupt, 0);		//use the last IPC channel to comm..
	irqSet(IRQ_TIMER1, soundinterrupt);
	irqSet(IRQ_LID, lidinterrupt);
}
