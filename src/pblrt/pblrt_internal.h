#ifndef PBLRT_INTERNAL_H
#define PBLRT_INTERNAL_H

#include "pebble/pebble.h"
#include "pblrt_drivers.h"
#include "pblrt_inmgr.h"
#include "opt/fs/fs.h"
#include "opt/serial/serial.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>

#define PBLRT_INPUT_COUNT 4

extern struct pblrt {

  // pblrt_configure.c
  const char *exename;
  const char *rompath;
  char *video_driver;
  char *video_device;
  int fullscreen;
  int video_w,video_h;
  char *audio_driver;
  char *audio_device;
  int audio_rate;
  int audio_chanc;
  int audio_buffer;
  char *input_driver;
  int lang;
  
  // pblrt_romsrc.c
  const char *romname; // Prefer for logging, after pblrt_romsrc_init
  const void *rom;
  int romc;
  int ownrom;
  
  // pblrt_rom.c
  int metadatap; // If nonzero, metadata:1 is validated and properly terminated. Skips the signature.
  
  volatile int terminate;
  int termstatus;
  struct pblrt_video *video;
  struct pblrt_audio *audio;
  struct pblrt_input *input;
  int instate[PBLRT_INPUT_COUNT];
  char *title_storage; // Needs to be NUL-terminated and sanitized before sending to driver.
  int fbw,fbh;
  struct pblrt_inmgr inmgr;
  
  double clock_recent;
  double starttime_real;
  double starttime_cpu;
  int framec;
  int clock_faultc;
  
  /* Circular audio buffer.
   * Driver requests audio on its own time, typically from a background thread.
   * We serve those requests from this buffer.
   * Between video frames, we carefully lock the driver and call out to the client to refill.
   */
  int16_t *abuf;
  int abufp,abufc,abufa;
  int synth_limit;
  
} pblrt;

int pblrt_configure(int argc,char **argv);
void pblrt_print_usage();
int pblrt_choose_default_language(); // Lives with configure, but must defer until after rom loads.

int pblrt_romsrc_init();
void pblrt_romsrc_quit();

int pblrt_exec_init();
void pblrt_exec_quit();
int pblrt_exec_update(double elapsed);
int pblrt_exec_call_init();
void pblrt_exec_call_quit();
int pblrt_exec_call_render(void **fbpp);
int pblrt_exec_refill_audio();

void pblrt_clock_init();
double pblrt_clock_update();
int pblrt_clock_report(double *rate,double *cpu); // Returns framec. If >0, the other things are populated too.

int pblrt_rom_get(void *dstpp,uint8_t tid,uint16_t rid);
int pblrt_rom_get_metadata(void *dstpp,const char *k,int kc); // Returns exactly what's in metadata:1
int pblrt_rom_get_metadata_text(void *dstpp,const char *k,int kc); // Resolves strings as appropriate. (k) should not include the trailing '@' or '$'.
int pblrt_rom_get_string(void *dstpp,int rid,int index); // (rid) in (1..63), language applies automatically

void pblrt_cb_close();
void pblrt_cb_focus(int focus);
void pblrt_cb_key(int keycode,int value);
void pblrt_cb_pcm(int16_t *v,int c);
void pblrt_cb_connect(int devid);
void pblrt_cb_disconnect(int devid);
void pblrt_cb_button(int devid,int btnid,int value);

void pblrt_lang_changed();

#endif
