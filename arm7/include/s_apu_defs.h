// ===============================
// NES APU REGISTERS ($4000–$4017)
// ===============================
// https://www.nesdev.org/wiki/APU

// Pulse 1
#define APU_PULSE1_CTRL      0x4000
#define APU_PULSE1_SWEEP     0x4001
#define APU_PULSE1_TIMER_L   0x4002
#define APU_PULSE1_TIMER_H   0x4003

// Pulse 2
#define APU_PULSE2_CTRL      0x4004
#define APU_PULSE2_SWEEP     0x4005
#define APU_PULSE2_TIMER_L   0x4006
#define APU_PULSE2_TIMER_H   0x4007

// Triangle
#define APU_TRI_CTRL         0x4008
#define APU_TRI_UNUSED       0x4009
#define APU_TRI_TIMER_L      0x400A
#define APU_TRI_TIMER_H      0x400B

// Noise
#define APU_NOISE_CTRL       0x400C
#define APU_NOISE_UNUSED     0x400D
#define APU_NOISE_PERIOD     0x400E
#define APU_NOISE_LENGTH     0x400F

// DMC
#define APU_DMC_CTRL         0x4010
#define APU_DMC_LOAD         0x4011
#define APU_DMC_ADDR         0x4012
#define APU_DMC_LENGTH       0x4013

// Status & Frame Counter
#define APU_STATUS           0x4015
#define APU_FRAME_COUNTER    0x4017

// $4000 / $4004 – Control (Duty / Envelope)
#define PULSE_DUTY_MASK      0xC0  // Bits 6–7
#define PULSE_ENV_LOOP       0x20  // Envelope loop / length counter halt
#define PULSE_ENV_CONST_VOL  0x10  // Constant volume
#define PULSE_VOLUME_MASK    0x0F

// $4001 / $4005 – Sweep
#define PULSE_SWEEP_ENABLE   0x80
#define PULSE_SWEEP_PERIOD   0x70  // Bits 4–6
#define PULSE_SWEEP_NEGATE   0x08
#define PULSE_SWEEP_SHIFT    0x07

// $4003 / $4007 – Length counter load
#define PULSE_LENGTH_MASK    0xF8  // Bits 3–7
#define PULSE_LENGTH_RELOAD  0x0FF
#define PULSE_TIMER_LOW      0x700
#define PULSE_TIMER_HIGH     0x07

// $4008 – Linear counter
#define TRI_LINEAR_HALT      0x80
#define TRI_LINEAR_LOAD      0x7F
#define TRI_VOLUME_MASK      0x0F

// $400B – Length / Timer high
#define TRI_LENGTH_MASK      0xF8
#define TRI_LENGTH_RELOAD    0x0FF
#define TRI_TIMER_LOW        0x700
#define TRI_TIMER_HIGH       0x07

// $400C – Envelope
#define NOISE_ENV_LOOP       0x20
#define NOISE_ENV_CONST      0x10
#define NOISE_VOLUME_MASK    0x0F

// $400E – Period / Mode
#define NOISE_MODE           0x80  // 0=long, 1=short
#define NOISE_PERIOD_MASK    0x0F

// $400F – Length counter load
#define NOISE_LENGTH_MASK    0xF

// $4010 – Control
#define DMC_IRQ_ENABLE       0x80
#define DMC_LOOP             0x40
#define DMC_RATE_MASK        0x0F

// $4011 – DAC Load
#define DMC_DAC_OUT          0x3F
#define DMC_DAC_MASK         0x7F
#define DMC_DAC_RELOAD       0xFF

// Status Register $4015
#define APU_STATUS_DMC_IRQ   0x80
#define APU_STATUS_FRAME_IRQ 0x40

#define APU_CH_PULSE1        0x01
#define APU_CH_PULSE2        0x02
#define APU_CH_TRIANGLE      0x04
#define APU_CH_NOISE         0x08
#define APU_CH_DMC           0x10

// Frame Counter $4017
#define APU_FRAME_5STEP      0x80
#define APU_FRAME_IRQ_OFF    0x40

// ===============================
// NES EXTRA SOUND CHIP REGISTERS ($4017+)
// ===============================

// FDS (Famicom Disk System)
#define FDS_BASE             0x4040
#define FDS_WAVE_RAM         0x4040  // 64 bytes
#define FDS_ENV_SPEED        0x408A
#define FDS_FREQ_L           0x4082
#define FDS_FREQ_H           0x4083
#define FDS_END              0x4040

// VRC6 (Konami)
#define VRC6_MIN_BASE        0x8000
// Pulse channels
#define VRC6_PULSE1_CTRL     0x9000
#define VRC6_PULSE1_FREQ_L   0x9001
#define VRC6_PULSE1_FREQ_H   0x9002

#define VRC6_PULSE2_CTRL     0xA000
#define VRC6_PULSE2_FREQ_L   0xA001
#define VRC6_PULSE2_FREQ_H   0xA002

// Saw
#define VRC6_SAW_RATE        0xB000
#define VRC6_SAW_FREQ_L      0xB001
#define VRC6_SAW_FREQ_H      0xB002
