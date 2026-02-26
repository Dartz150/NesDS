#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// -- NAMCO 163 Sound Handlers --

/// @brief Inits the N163 sound expansion sound hardware status.
/// @param nes_apu_clock NES APU clock in cycles.
/// @param ds_sound_freq Nintendo DS Hardware Sound Frequency, should be 32768.
void n163SoundInit(Uint32 nes_apu_clock, Uint32 ds_sound_freq);

/// @brief Updates the DS sound hardware timers each time the internal N163 sound status changes.
/// @param nes_apu_clock NES APU clock in cycles.
/// @param ds_sound_freq Nintendo DS Hardware Sound Frequency, should be 32768.
void n163SoundHwUpdate(Uint32 nes_apu_clock, Uint32 ds_sound_freq);

/// @brief Stops the DS hardware that is currently playing the MMC5 sound expamsion channels.
void n163SoundHwStop();

/// @brief N163 Register dispatcher.
/// @param address Location of the value.
/// @param value Value written.
void __fastcall n163SoundWrite(Uint address, Uint value);

#ifdef __cplusplus
}
#endif