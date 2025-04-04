#include <nds.h>
#include "c_defs.h"
#include "NesMachine.h"

extern u32 agb_bg_map[];
u32 debuginfo[48];
char *debugtxt[]={
"ERR0","ERR1","READ","WRITE","BRK","BAD OP","VBLS","FPS",
"BGMISS","cartflg","a","b","c","ALIVE","TMP0","TMP1",
"mapper#", "PRGCRC", "diskno", "makeid", "gameid", "emuflag"};
#define DLINE 4

extern int shortcuts_tbl[];
extern char *ishortcuts[];

char *fdsdbg[]={
"ienable", "irepeat", "ioccur", "itran",
"denable", "senable", "RWstart", "RWmode",
"motormd", "eject", "ready", "reset",
"faccess", "side", "mountct", "itype",
"startupf", "Throttle",
"Thtime", "disk", "disk_w", "icount",
"ilatch", "bpoint", "bmode", "fsize",
"famount", "point", "sttimer", "sktimer",
"diskno", "makerid", "gameid",
};

// Function to get the sound mode string based on ExtraChipSelect value
const char* getSoundModeString(unsigned char extraChipSelect) 
{
    if (extraChipSelect == NO_EXTRA_CHIP) 
	{
        return "  None";
    } else {
        // Check each individual bit to determine the sound mode
        if (extraChipSelect & VRC6_AUDIO) {
            return " VRC6";
        } else if (extraChipSelect & VRC7_AUDIO) {
            return " VRC7";
        } else if (extraChipSelect & FDS_AUDIO) {
            return " FDS";
        } else if (extraChipSelect & MMC5_AUDIO) {
            return " MMC5";
        } else if (extraChipSelect & NAMCO_163_AUDIO) {
            return " Namco 163";
        } else if (extraChipSelect & SUNSOFT_5B_AUDIO) {
            return " Sunsoft 5B";
        } else if (extraChipSelect & VT02P_AUDIO) {
            return " VT02+";
        } else {
            return " Unknown";
        }
    }
}

// TODO: ADD Graphical Objects
const char* getPlayStatusIcon(unsigned char playingFlag) {
    return playingFlag == 0x01 ? ">" : "  ";
}

int debugdump() {
/*
	int i;
	u32 *p;
	char **cc;

	debuginfo[ALIVE]=IPC_ALIVE;
	debuginfo[TMP0]=IPC_TMP0;
	debuginfo[TMP1]=IPC_TMP1;
	
	p=debuginfo;
	cc=debugtxt;
	for(i=64*DLINE;i<64*(DLINE+16);i+=64) {
		consoletext(i,*cc++,0);
		hex32(i+14,*p++);
	}

	p=agb_bg_map;
	for(i=64*DLINE;i<64*(DLINE+16);i+=64) {
		hex32(i+32,*p++);
	}
	
	hex32(64*22,freemem_end-freemem_start);
	consoletext(64*22+18,"bytes free",0);
	consoletext(64*23,"Built",0);
	consoletext(64*23+12,__TIME__,0);
	consoletext(64*23+30,__DATE__,0);
*/

	int i;/*
	static int count = 0;
	if(count++ != 20)
		return 0;*/

	debuginfo[EMUFLAG] = __emuflags;
#ifdef DEBUG
	debuginfo[BRK] = globals.cpu.brkCount;
	debuginfo[BADOP] = globals.cpu.badOpCount;
#endif
	if(1 && (__emuflags & NSFFILE)) {
		u32 *ip=(u32*)&globals.mapperData;
		consoletext	(64 * 4 + 0 * 32, "Version:", 0);
		hex8		(64 * 4 + 0 * 32 + 18, nsfHeader.Version);
		consoletext	(64 * 4 + 1 * 32, "startson", 0);
		hex8		(64 * 4 + 1 * 32 + 18, nsfHeader.StartSong);
		consoletext	(64 * 4 + 2 * 32, "totalsong", 0);
		dec		(64 * 4 + 2 * 32 + 18, (nsfHeader.TotalSong),2);
		consoletext	(64 * 4 + 3 * 32, "LoadAddr", 0);
		hex16		(64 * 4 + 3 * 32 + 18, nsfHeader.LoadAddress);
		consoletext	(64 * 4 + 4 * 32, "InitAddr", 0);
		hex16		(64 * 4 + 4 * 32 + 18, nsfHeader.InitAddress);
		consoletext	(64 * 4 + 5 * 32, "PlayAddr", 0);
		hex16		(64 * 4 + 5 * 32 + 18, nsfHeader.PlayAddress);

		// NSF Info
		consoletext	(64 * 4 + 8 * 32, "Name:", 0);
		consoletext	(63 * 4 + 8 * 32 + 18, nsfHeader.SongName, 0);
		consoletext	(64 * 4 + 10 * 32, "Artist:", 0);
		consoletext	(63 * 4 + 10 * 32 + 20, nsfHeader.ArtistName, 0);
		consoletext	(64 * 4 + 12 * 32, "CopyRight:", 0);
		consoletext	(63 * 4 + 12 * 32 + 25, nsfHeader.CopyrightName, 0);
		consoletext	(64 * 4 + 14 * 32, "XtraChip:", 0);
		consoletext	(63 * 4 + 14 * 32 + 22, (char*)getSoundModeString(nsfHeader.ExtraChipSelect), 0);

		// consoletext	(64 * 4 + 14 * 32, "Expansion:", 0);
		// consoletext	(63 * 4 + 14 * 32 + 18, nsfHeader.Expansion, 0);
		
		// //for debugging
		// for(i=0;i<15;i++) {
		// 	hex32(64*7+i*32,ip[i]);
		// }
		consoletext	(64 * 16 + 0 * 32, "Song No:", 0);
		dec		(64 * 16 + 0 * 32 + 18, (__nsfSongNo + 1),2);
		consoletext	(64 * 16 + 1 * 32, "Mode:", 0);
		hex8		(64 * 16 + 1 * 32 + 18, __nsfSongMode);
		consoletext	(64 * 16 + 2 * 32, "Playing:", 0);
		consoletext		(64 * 16 + 2 * 32 + 18, (char*)getPlayStatusIcon(__nsfPlay), 0);

		
		for(i = 0; i < 4; i++) {
			hex32(64 * 20 + i * 32, (u32)rp2A03.m6502.memTbl[i + 4] + 0x2000 * i + 0x8000);
		}
	} else if(debuginfo[MAPPER] == 20) {
		u8 *p=(u8*)&globals.mapperData;//0x7000000;
		u32 *ip=(u32*)&globals.mapperData;//0x7000000;
		for(i=0;i<18;i++) {
			consoletext(64 * 4 + i * 32, fdsdbg[i], 0);
			hex8(64*4+i*32 + 18,p[i]);
		}
		for(i=6;i<19;i++) {
			consoletext(64*12 + (i-6) * 32, fdsdbg[i + 12], 0);
			hex32(64*12 + (i-6) * 32 + 16,ip[i]);
		}
	}
	else {
		for(i = 0; i < 22; i++) {
			consoletext(64 * 4 + i * 32, debugtxt[i], 0);
			hex32(64 * 4 + i * 32 + 14, debuginfo[i]);
		}
#if 1
		for(i = 0; i < 8; i++) {
			hex(64 * 15 + i * 8, globals.ppu.nesChrMap[i], 2);
		}
		for(i = 0; i < 4; i++) {
			hex(64 * 16 + i * 8, ((char *)rp2A03.m6502.memTbl[i + 4] - (char *)globals.romBase)/0x2000 + i + 4, 2);
		}
#endif
#if 1
		for (i = 0;i < 96; i++)
		{
			hex8(64*17 + i*8, globals.mapperData[i]);
		}
#endif
	}
	return 0;
}
