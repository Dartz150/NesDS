#pragma once
#ifndef C_DEFS_H
#define C_DEFS_H

//FIFO_USER_06	for ipc_region
//FIFO_USER_07	for audio data
//FIFO_USER_08  for APU control

//#define DTCM 0x0b000000

#define NES_RAM nes_region
#define NES_SRAM NES_RAM + 0x800
//nes_region
#define MAX_IPS_SIZE 		0x80000		//actually, the ips file won't be larger than 512kB.
#define ROM_MAX_SIZE 		0x2b0000		//2,7MB free rom space
#define MAXFILES 			1024

#define VRAM_ABCD 			(*(vu32*)0x4000240)
#define VRAM_EFG 			(*(vu32*)0x4000244)
#define VRAM_HI 			(*(vu16*)0x4000248)

//IPC_* also in equates.h, KEEP BOTH UPDATED
#undef IPC
#define IPC 				((u8 *)ipc_region)
#define IPC_TOUCH_X			(*(vu32*)(IPC+0))
#define IPC_TOUCH_Y			(*(vu32*)(IPC+4))
#define IPC_KEYS			(*(vu32*)(IPC+8))
#define IPC_ALIVE			(*(vu32*)(IPC+12))					//unused anymore
#define IPC_MEMTBL  		((char **)(IPC+16))
#define IPC_REG4015 		(*(char *)(IPC+32))					//arm7 should not write this value but to use fifo. channel 5
#define IPC_APUIRQ  		(*(char *)(IPC+33))					//not supported.
#define IPC_RAWPCMEN 		(*(char *)(IPC+34))					//not supported.
#define IPC_APUW 			(*(volatile int *)(IPC+40))			//apu write start
#define IPC_APUR 			(*(volatile int *)(IPC+44))			//apu write start
#define IPC_MAPPER 			(*(volatile int *)(IPC+48))			//nes rom mapper
#define IPC_PCMDATA			(u8 *)(IPC+128)						//used for raw pcm.
#define IPC_APUWRITE 		((unsigned int *)(IPC+512))			//apu write start
#define IPC_AUDIODATA 		((unsigned int *)(IPC+4096 + 512))	//audio data...

//not implemented yet.

#undef KEY_TOUCH
#define KEY_TOUCH 0x1000
#define KEY_CLOSED 0x2000

// #define FIFO_WRITEPM 		1
// #define FIFO_APU_PAUSE 		2
// #define FIFO_UNPAUSE 		3
// #define FIFO_APU_RESET 		4
// #define FIFO_SOUND_RESET 	5
// #define FIFO_APU_PAL 	 	6
// #define FIFO_APU_NTSC     	7
// #define FIFO_APU_SWAP 		8
// #define FIFO_APU_NORM 		9
// #define FIFO_SOUND_UPDATE	10
// #define FIFO_AUDIO_FILTER 	11

#ifdef ARM9

/** Rom info from builder */
typedef struct {
	char name[32];
	u32 filesize;
	u32 flags;
	u32 spritefollow;
	u32 reserved;
} romheader;	

#define SAVESTATESIZE (0x2800+0x3000+96+64+16+96+16+44+64)


//arm9main.c
extern int soft_frameskip;
extern int palette_value;
extern int global_playcount;
extern int subscreen_stat;
void showversion();
/** Run 1 NES frame with FF/REW control */
void play(void);
void recorder_reset(void);

//console.c
#define SUB_CHR 0x6204000
#define SUB_BG 0x6200000
void consoleinit(void);
#define hex8(a,b) hex(a,b,1)
#define hex16(a,b) hex(a,b,3)
#define hex24(a,b) hex(a,b,5)
#define hex32(a,b) hex(a,b,7)
void hex(int offset,int d,int n);
#define dec1(a,b) dec(a,b,1)
#define dec10(a,b) dec(a,b,2)
#define dec100(a,b) dec(a,b,3)
#define dec1000(a,b) dec(a,b,4)
void dec(int offset, int d, int n);
void consoletext(int offset,char *s,int color);
void menutext(int line,char *s,int selected);
void clearconsole(void);
void hideconsole(void);
void showconsole(void);
void soundHardReset();
void SoundUpdate();

//subscreen.c
int debugdump(void);

#define ERR0 0
#define ERR1 1
#define READ 2
#define WRITE 3
#define BRK 4
#define BADOP 5
#define VBLS 6
#define FPS 7
#define BGMISS 8
#define CARTFLAG 9

#define ALIVE 13
#define TMP0 14
#define TMP1 15
#define MAPPER 16
#define PRGCRC 17
#define DISKNO 18
#define MAKEID 19
#define GAMEID 20
#define EMUFLAG 21

//romloader.c
extern int romsize;
int bootext();
extern int active_interface;
extern char romfilename[256];
extern char *romfileext;
void do_rommenu(void);
extern int freemem_start;
//#define freemem_end 0x23e0000				//this cannot be used based on libnds
extern int freemem_end;

extern char inibuf[768];
extern char ininame[768];

//minIni.c
long ini_getl(const char *Section, const char *Key, long DefValue, const char *Filename);
int ini_puts(const char *Section, const char *Key, const char *Value, const char *Filename);
int ini_putl(const char *Section, const char *Key, long Value, const char *Filename);
int ini_gets(const char *Section, const char *Key, const char *DefValue, char *Buffer, int BufferSize, const char *Filename);

//romloader_frontend.c
bool readFrontend(char *target);


//misc.c
#define MAX_SC 19

extern char *ishortcuts[];
extern char *igestures[];
extern char *hshortcuts[];
extern int shortcuts_tbl[];
extern char *keystrs[];
extern char gestures_tbl[][32];
extern int do_gesture_type;
void do_quickf(int func);

extern int slots_num;
extern int screen_swap;
extern bool use_saves_dir;
typedef struct {
	int offset;
	char *str;
} touchstring;
void do_shortcuts();
extern int touchstate, last_x, last_y;
void touch_update(void);
int do_touchstrings(touchstring*,int pushstate);
void load_sram(void);
void save_sram(void);
void write_savestate(int num);
u32 read_savestate(u32 num);
void ARM9sleep(void);
void reg4015interrupt(u32 msg, void *none);
void fdscmdwrite(u8 diskno);
void Sound_reset();
//*.s

//emuFlags:
#define NOFLICKER 1
#define ALPHALERP 2
//joyflags
#define B_A_SWAP	0x80000
#define L_R_DISABLE	0x100000
//cartFlags
#undef SRAM
#define SRAM 0x02

#define __emuflags globals.emuFlags
#define __cartflags globals.cartFlags

extern u32 joyflags;		//io.s
extern u32 joystate;

#define __scanline globals.ppu.scanline
extern u8 __barcode;
extern u8 __barcode_out;
extern u32 __af_state;
extern u32 __af_start;
extern u32 __nsfPlay;
extern u32 __nsfInit;
extern u32 __nsfSongNo;
extern u32 __nsfSongMode;

// NTSC/PAL bits definitions (NTSC_PALbits)
#define NTSC_TUNE           0x00 // If clear, this is an NTSC tune
#define PAL_TUNE            0x01 // If set, this is a PAL tune
#define DUAL_NTSC_PAL_TUNE  0x02 // If set, this is a dual PAL/NTSC tune
// Bits 3-7 are reserved and must be 0

// Extra sound chip select definitions (ExtraChipSelect)
#define NO_EXTRA_CHIP       0x00
#define VRC6_AUDIO          0x01
#define VRC7_AUDIO          0x02
#define FDS_AUDIO           0x04
#define MMC5_AUDIO          0x08
#define NAMCO_163_AUDIO     0x10
#define SUNSOFT_5B_AUDIO    0x20
#define VT02P_AUDIO         0x40

extern struct nsfHeader
{
    char ID[5];                     // 'N','E','S','M',$1A (denotes an NES sound format file)
    char Version;                   // Version number $01 (or $02 for NSF2)
    char TotalSong;                 // Total songs (1=1 song, 2=2 songs, etc)
    char StartSong;                 // Starting song (1=1st song, 2=2nd song, etc)
    unsigned short LoadAddress;     // (lo, hi) load address of data ($8000-FFFF)
    unsigned short InitAddress;     // (lo, hi) init address of data ($8000-FFFF)
    unsigned short PlayAddress;     // (lo, hi) play address of data ($8000-FFFF)
    char SongName[32];              // The name of the song, null terminated
    char ArtistName[32];            // The artist, if known, null terminated
    char CopyrightName[32];         // The copyright holder, null terminated
    unsigned short SpeedNTSC;       // (lo, hi) Play speed, in 1/1000000th sec ticks, NTSC (see text)
    char BankSwitch[8];             // Bankswitch init values (see text, and FDS section)
    unsigned short SpeedPAL;        // (lo, hi) Play speed, in 1/1000000th sec ticks, PAL (see text)
    char NTSC_PALbits;              // PAL/NTSC bits
                                     // bit 0: if clear, this is an NTSC tune
                                     // bit 1: if set, this is a PAL tune
                                     // bit 2: if set, this is a dual PAL/NTSC tune
                                     // bits 3-7: reserved, must be 0
    unsigned char ExtraChipSelect;  // Extra Sound Chip Support
                                     // bit 0: if set, this song uses VRC6 audio
                                     // bit 1: if set, this song uses VRC7 audio
                                     // bit 2: if set, this song uses FDS audio
                                     // bit 3: if set, this song uses MMC5 audio
                                     // bit 4: if set, this song uses Namco 163 audio
                                     // bit 5: if set, this song uses Sunsoft 5B audio
                                     // bit 6: if set, this song uses VT02+ audio
                                     // bit 7: reserved, must be zero
    char Expansion[4];              // Reserved for NSF2 (must be 0)
} nsfHeader;


void EMU_VBlank(void);
void EMU_Run(void);
void NSF_Run(void);
void initcart(char *rom);//,int flags);
void NES_reset(void);		//cart.s
int savestate(u32);
int loadstate(u32);
void rescale(int a, int b);

//render.s
#define __rendercount globals.renderCount
extern void render_all();
extern void render_sub();


//multi.c
extern void initNiFi();
extern void do_multi();
extern int nifi_cmd;
extern int nifi_stat;
extern int nifi_menu();

//cheat.c
extern int do_cheat();
extern int cheatlist();
extern int addcheat();
extern void load_cheat(void);
extern void save_cheat(void);

//ips.c
void load_ips(const char *name);
void do_ips(int romsize);
extern bool ips_stat;

//gesture.c
void do_gesture(void);
int get_gesture(int out);
extern int gesture;
extern char gesture_combo[32];
extern int gesture_pos;

//about.c
extern int nesds_about();

//barcode.c
extern void setbarcodedata( char *code, int len );

//memory.s
extern u8 rom_start[];
extern u8 rom_files[];
extern u32 ipc_region[];
extern u8 nes_region[];
extern u8 ct_buffer[];

//menu.h
void do_menu();

//menu_func.c
extern int ad_scale, ad_ypos;
extern int autofire_fps;
extern u8 nes_rgb[];

//subscreen.c
extern u32 debuginfo[48];

//others...
#define all_pix_start globals.ppu.pixStart
#define all_pix_end globals.ppu.pixEnd

//rompatch.c
extern void crcinit();
extern void romcorrect(char *s);

//zip
extern int load_gz(const char *fname);
int do_decompression(const char *inname, const char *outname);

#define MP_KEY_MSK		0x0CFF		
#define MP_HOST			(1 << 31)		
#define MP_CONN			(1 << 30)		
#define MP_RESET		(1 << 29)
#define MP_NFEN			(1 << 28)

#define MP_TIME_MSK		0xFFFF		
#define MP_TIME			16	

#define P1_ENABLE 0x10000
#define P2_ENABLE 0x20000
#define B_A_SWAP 0x80000
#define L_R_DISABLE 0x100000
#define AUTOFIRE 0x1000000
#define MIRROR 0x01 //horizontal mirroring
#define SRAM 0x02 //save SRAM
#define TRAINER 0x04 //trainer present
#define SCREEN4 0x08 //4way screen layout
#define VS 0x10 //VS unisystem
#define NOFLICKER 1	//flags&3:  0=flicker 1=noflicker 2=alphalerp
#define ALPHALERP 2
#define PALTIMING 4	//0=NTSC 1=PAL
#define FOLLOWMEM 32  //0=follow sprite, 1=follow mem
#define SPLINE 64
#define SOFTRENDER 128	//pure software rendering
#define ALLPIXEL 256 // use both screens to show the pixels
#define NEEDSRAM 512	//will autoly save sram 3 second after sram write.
#define AUTOSRAM 1024	//enable auto saving sram.
#define SCREENSWAP 2048
#define LIGHTGUN 4096	//lighting gun
#define MICBIT 8192	//Mic bit
#define PALSYNC	16384 //for palette sync
#define STRONGSYNC 32768 //for scanline sync
#define SYNC_NEED 0x18000 //need for palette sync, updated when there is writings on pal. OR STRONGSYNC is set.
#define PALSYNC_BIT	0x10000 //for pal sync. as a sign
#define FASTFORWARD 0x20000 //for fast forward
#define REWIND 0x40000 // for backward
#define ALLPIXELON 0x80000 //on or off state of all_pix_show
#define NSFFILE 0x100000 //on or off state of all_pix_show
#define DISKBIOS 0x200000 // if diskbios was loaded
#else // ARM9
extern int ipc_region;
#endif // ARM9

#endif
