#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// -- MMC5 Sound Handlers -- 

/// @brief Inits the MMC5 sound expansion sound hardware status.
void mmc5SoundInit();

/// @brief Updates the DS sound hardware timers each time the internal MMC5 sound status changes.
/// @param nes_apu_clock NES APU clock in cycles.
/// @param ds_sound_freq Nintendo DS Hardware Sound Frequency, should be 32768.
void mmc5SoundHwUpdate(Uint32 nes_apu_clock, Uint32 ds_sound_freq);

/// @brief Stops the DS hardware that is currently playing the MMC5 sound expamsion channels.
void mmc5SoundHwStop();

/// @brief MMC5 Register dispatcher.
/// @param address Location of the value.
/// @param value Value written.
void mmc5SoundWrite(Uint address, Uint value);

/// @brief Called in the main loop to sync the PCM writes, we need it to be reset each DS frame
void mmc5VblankSync();

#ifdef __cplusplus
}
#endif