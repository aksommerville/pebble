#ifndef LOFI_INTERNAL_H
#define LOFI_INTERNAL_H

#include "lofi.h"
#include "stdlib/egg-stdlib.h"

/* These limits are tweakable, within reason.
 */
#define LOFI_WAVE_SIZE_BITS 9
#define LOFI_WAVE_LIMIT 8
#define LOFI_VOICE_LIMIT 16
#define LOFI_RATE_MIN 200
#define LOFI_RATE_MAX 200000
#define LOFI_CHANC_MIN 1
#define LOFI_CHANC_MAX 8
#define LOFI_DUR_LIMIT_MS 10000 /* Important: LOFI_DUR_LIMIT_MS*LOFI_RATE_MAX must not exceed INT_MAX. Hard limit 10737. */

#define LOFI_WAVE_SIZE_SAMPLES (1<<LOFI_WAVE_SIZE_BITS)
#define LOFI_WAVE_SHIFT (32-LOFI_WAVE_SIZE_BITS)
#define LOFI_CHANNEL_COUNT 8 /* Song format assumes 8, not trivial to change. */

struct lofi_wave {
  int16_t v[LOFI_WAVE_SIZE_SAMPLES];
};

// Values are u0.8, times are frames.
// Since we always receive the duration on note start, sustain is predetermined.
struct lofi_env {
  uint8_t a,z;
  uint8_t atkv,susv;
  int p,c;
  int atkt,dect,sust,rlst;
  int stage; // (0,1,2,3,4)=(done,attack,decay,sustain,release)
};

struct lofi_voice {
  struct lofi_wave *wave;
  uint32_t p;
  uint32_t dp;
  uint32_t ddp;
  int ttl;
  struct lofi_env env;
};

extern struct lofi {
  int rate,chanc;
  struct lofi_wave sine;
  struct lofi_wave wavev[LOFI_WAVE_LIMIT];
  struct lofi_voice voicev[LOFI_VOICE_LIMIT];
  int voicec;
  uint32_t step_by_noteid[128];
  
  // (song==null) is the one and only indicator whether a song is playing.
  struct lofi_channel {
    uint8_t program; // 0xf8=env 0x07=wave
    uint8_t trim;
  } channelv[LOFI_CHANNEL_COUNT];
  const uint8_t *song; // Start of events; we strip the header at load.
  int songp,songc;
  int songdelay; // frames;
} lofi;

/* Adds to (v).
 */
void lofi_voice_update(int16_t *v,int c,struct lofi_voice *voice);

#endif
