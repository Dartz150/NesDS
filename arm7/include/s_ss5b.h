#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// -- SunSoft 5B Sound Handlers -- 

/// @brief Inits the SunSoft 5B sound expansion sound hardware status.
void ss5bSoundInit();

/// @brief Updates the DS sound hardware timers each time the internal SunSoft 5B sound status changes.
/// @param nes_apu_clock NES APU clock in cycles.
/// @param ds_sound_freq Nintendo DS Hardware Sound Frequency, should be 32768.
void ss5bSoundHwUpdate(Uint32 nes_apu_clock, Uint32 ds_sound_freq);

/// @brief Stops the DS hardware that is currently playing the SunSoft 5B sound expamsion channels.
void ss5bSoundHwStop();

/// @brief SS5B Register dispatcher.
/// @param address Location of the value.
/// @param value Value written.
void ss5bSoundWrite(Uint address, Uint value);

#ifdef __cplusplus
}
#endif