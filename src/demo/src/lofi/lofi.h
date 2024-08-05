/* lofi.h
 * Minimal synthesizer for Pebble (but usable anywhere).
 * We use egg-stdlib.
 */
 
#ifndef LOFI_H
#define LOFI_H

#include <stdint.h>

void lofi_quit();

int lofi_init(int rate,int chanc);

/* Initialize waves immediately after global init.
 * Safe to change after, but voices in flight may pop.
 */
int lofi_wave_init_sine(int waveid);
int lofi_wave_init_square(int waveid);
int lofi_wave_init_saw(int waveid);
int lofi_wave_init_triangle(int waveid);
int lofi_wave_init_harmonics(int waveid,const uint8_t *coefv,int coefc);

void lofi_update(int16_t *v,int c);

/* Start a note.
 * May evict a running note, if we're at the voice limit.
 */
void lofi_note(uint8_t program,uint8_t trim,uint8_t noteida,uint8_t noteidz,int durms);

/* Stop any current song and replace with this.
 * (0,0) to only turn off current song.
 * We'll repeat at the end.
 * Caller must hold (src) constant for as long as it plays.
 */
void lofi_play_song(const void *src,int srcc);

/* Truncate sustain of all running voices so they finish soon.
 * This happens automatically when you change songs.
 */
void lofi_release_all();

#endif
