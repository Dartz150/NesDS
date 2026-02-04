#ifndef SOUNDIPC_H__
#define SOUNDIPC_H__

#ifdef __cplusplus
extern "C" {
#endif

// To comunicate with ARM7 sound states
#define FIFO_WRITEPM 		   1
#define FIFO_APU_PAUSE 		   2
#define FIFO_UNPAUSE 		   3
#define FIFO_APU_RESET 		   4
#define FIFO_SOUND_RESET 	   6
#define FIFO_SOUND_UPDATE  	   7

#define FIFO_APU_UPDATE_FLAGS  0x20 // Command for APU flags

// Sound Config. bits
#define APU_STAT_REGION_PAL    (1 << 0)  // 0: NTSC, 1: PAL
#define APU_STAT_DUTY_REV      (1 << 1)  // 0: Normal, 1: Reverse
#define APU_STAT_PULSE_HW      (1 << 2)  // 0: SW, 1: HW PSG
#define APU_STAT_STEREO        (1 << 3)  // 0: Mono, 1: Stereo

// Channel mute bits (0: On, 1: Mute)
#define APU_STAT_MUTE_P1       (1 << 4)
#define APU_STAT_MUTE_P2       (1 << 5)
#define APU_STAT_MUTE_TRI      (1 << 6)
#define APU_STAT_MUTE_NOI      (1 << 7)
#define APU_STAT_MUTE_DMC      (1 << 8)
// Sound Expansion Channels
#define APU_STAT_MUTE_FDS      (1 << 9)
#define APU_STAT_MUTE_VRC_P1   (1 << 10)
#define APU_STAT_MUTE_VRC_P2   (1 << 11)
#define APU_STAT_MUTE_VRC_SAW  (1 << 12)

// Extra configs.
#define APU_STAT_REGION_DDY    (1 << 13)  // 0: Ignore, 1 Dendy mode

#ifdef __cplusplus
}
#endif

#endif /* SOUNDIPC_H__ */