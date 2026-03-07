#ifndef SOUNDIPC_H__
#define SOUNDIPC_H__

#ifdef __cplusplus
extern "C" {
#endif

// To comunicate with ARM7 sound states
#define FIFO_WRITEPM 		     1
#define FIFO_APU_PAUSE 		     2
#define FIFO_UNPAUSE 		     3
#define FIFO_APU_RESET 		     4
#define FIFO_SOUND_RESET 	     6
#define FIFO_SOUND_UPDATE  	     7

#define FIFO_APU_UPDATE_FLAGS    0x20 // Command for APU flags

// Sound Config. bits
#define APU_STAT_REGION_PAL     (1 << 0)  // 0: NTSC, 1: PAL
#define APU_STAT_DUTY_REV       (1 << 1)  // 0: Normal, 1: Reverse
#define APU_STAT_PULSE_HW       (1 << 2)  // 0: Deprecated, HW render is now the main engine
#define APU_STAT_STEREO         (1 << 3)  // 0: Mono, 1: Stereo

// Extra configs.
#define APU_STAT_REGION_DDY     (1 << 21)  // 0: Ignore, 1 Dendy mode

// -- Standard APU Channels mute bits --
// (0: On, 1: Mute)
#define APU_STAT_MUTE_P1        (1 << 4)
#define APU_STAT_MUTE_P2        (1 << 5)
#define APU_STAT_MUTE_TRI       (1 << 6)
#define APU_STAT_MUTE_NOI       (1 << 7)
#define APU_STAT_MUTE_DMC       (1 << 8)

// -- Sound Expansion Channels mute bits --
// (0: On, 1: Mute)

// FDS
#define APU_STAT_MUTE_FDS       (1 << 9)

// VRC6
#define APU_STAT_MUTE_VRC_P1    (1 << 10)
#define APU_STAT_MUTE_VRC_P2    (1 << 11)
#define APU_STAT_MUTE_VRC_SAW   (1 << 12)

// MMC5
#define APU_STAT_MUTE_MMC5_P1   (1 << 13)
#define APU_STAT_MUTE_MMC5_P2   (1 << 14)
#define APU_STAT_MUTE_MMC5_PCM  (1 << 15)

// SS5B
#define APU_STAT_MUTE_SS5B_P1   (1 << 16)
#define APU_STAT_MUTE_SS5B_P2   (1 << 17)
#define APU_STAT_MUTE_SS5B_P3   (1 << 18)

// N163
#define APU_STAT_MUTE_N163      (1 << 19)

// VRC7
#define APU_STAT_MUTE_VRC7      (1 << 20)

#ifdef __cplusplus
}
#endif

#endif /* SOUNDIPC_H__ */