/* pblrt_drivers.h
 * The drivers state is global, it interacts directly with pblrt.
 * Implementations of (pblrt_video,pblrt_audio,pblrt_input) should be the only thing you need, 
 * to port Pebble's runtime to some new system. Famous last words.
 * Those implementations should live under src/opt/, and get included by the target Makefile.
 */
 
#ifndef PBLRT_DRIVERS_H
#define PBLRT_DRIVERS_H

#include <stdint.h>

struct pblrt_video;
struct pblrt_video_type;
struct pblrt_video_delegate;
struct pblrt_video_setup;
struct pblrt_audio;
struct pblrt_audio_type;
struct pblrt_audio_delegate;
struct pblrt_audio_setup;
struct pblrt_input;
struct pblrt_input_type;
struct pblrt_input_delegate;
struct pblrt_input_setup;

void pblrt_drivers_quit();

/* Attempts to stand all three drivers.
 * Video is required, you may assume it exists if this succeeds.
 * Audio is optional. We may leave it null.
 * Input is required only if the video driver does not claim to provide a keyboard.
 */
int pblrt_drivers_init();

int pblrt_drivers_update();

void pblrt_audio_play(int play);
int pblrt_audio_lock();
void pblrt_audio_unlock();

const struct pblrt_video_type *pblrt_video_type_by_index(int p);
const struct pblrt_video_type *pblrt_video_type_by_name(const char *src,int srcc);
const struct pblrt_audio_type *pblrt_audio_type_by_index(int p);
const struct pblrt_audio_type *pblrt_audio_type_by_name(const char *src,int srcc);
const struct pblrt_input_type *pblrt_input_type_by_index(int p);
const struct pblrt_input_type *pblrt_input_type_by_name(const char *src,int srcc);

struct pblrt_video *pblrt_video_new(const struct pblrt_video_type *type,const struct pblrt_video_delegate *delegate,const struct pblrt_video_setup *setup);
struct pblrt_audio *pblrt_audio_new(const struct pblrt_audio_type *type,const struct pblrt_audio_delegate *delegate,const struct pblrt_audio_setup *setup);
struct pblrt_input *pblrt_input_new(const struct pblrt_input_type *type,const struct pblrt_input_delegate *delegate,const struct pblrt_input_setup *setup);

void pblrt_video_del(struct pblrt_video *video);
void pblrt_audio_del(struct pblrt_audio *audio);
void pblrt_input_del(struct pblrt_input *input);

/* Instance and type bases, of interest mostly to driver implementations.
 *********************************************************************/
 
struct pblrt_video_delegate {
  void (*cb_close)();
  void (*cb_focus)(int focus);
  void (*cb_key)(int keycode,int value); // keycode is USB-HID
};

struct pblrt_video_setup {
  const char *device;
  int w,h; // Preferred window size. Zeroes are fine.
  int fbw,fbh; // Caller's framebuffer size, for guiding window size.
  int fullscreen;
  const char *title;
  const void *iconrgba;
  int iconw,iconh;
};
 
struct pblrt_video {
  const struct pblrt_video_type *type;
  struct pblrt_video_delegate delegate;
  int w,h; // Screen or window interior size.
  int fullscreen;
};

struct pblrt_video_type {
  const char *name; // REQUIRED
  const char *desc;
  int objlen; // REQUIRED; >=sizeof(struct pblrt_video)
  int appointment_only;
  // ^ don't change the first four fields please
  int provides_keyboard;
  void (*del)(struct pblrt_video *driver);
  int (*init)(struct pblrt_video *driver,const struct pblrt_video_setup *setup); // REQUIRED
  int (*update)(struct pblrt_video *driver);
  void (*set_fullscreen)(struct pblrt_video *driver,int fullscreen);
  int (*commit_framebuffer)(struct pblrt_video *driver,const void *rgba,int w,int h); // REQUIRED
  void (*skip)(struct pblrt_video *driver);
  void (*suppress_screensaver)(struct pblrt_video *driver);
};

struct pblrt_audio_delegate {
  void (*cb_pcm)(int16_t *v,int c);
};

struct pblrt_audio_setup {
  const char *device;
  int rate;
  int chanc;
  int buffer;
};

struct pblrt_audio {
  const struct pblrt_audio_type *type;
  struct pblrt_audio_delegate delegate;
  int rate,chanc;
  int playing;
};

struct pblrt_audio_type {
  const char *name; // REQUIRED
  const char *desc;
  int objlen; // REQUIRED; >=sizeof(struct pblrt_audio)
  int appointment_only;
  // ^ don't change the first four fields please
  void (*del)(struct pblrt_audio *driver);
  int (*init)(struct pblrt_audio *driver,const struct pblrt_audio_setup *setup); // REQUIRED
  void (*play)(struct pblrt_audio *driver,int play); // REQUIRED
  int (*update)(struct pblrt_audio *driver);
  int (*lock)(struct pblrt_audio *driver);
  void (*unlock)(struct pblrt_audio *driver);
};

struct pblrt_input_delegate {
  void (*cb_connect)(int devid);
  void (*cb_disconnect)(int devid);
  void (*cb_button)(int devid,int btnid,int value);
};

struct pblrt_input_setup {
  const char *path;
};

struct pblrt_input {
  const struct pblrt_input_type *type;
  struct pblrt_input_delegate delegate;
};

struct pblrt_input_type {
  const char *name; // REQUIRED
  const char *desc;
  int objlen; // REQUIRED; >=sizeof(struct pblrt_input)
  int appointment_only;
  // ^ don't change the first four fields please
  void (*del)(struct pblrt_input *driver);
  int (*init)(struct pblrt_input *driver,const struct pblrt_input_setup *setup); // REQUIRED
  int (*update)(struct pblrt_input *driver);
  int (*devid_by_index)(struct pblrt_input *driver,int p);
  const char *(*ids_by_devid)(int *vid,int *pid,int *version,struct pblrt_input *driver,int devid);
  
  // Drivers should indicate the most appropriate (pblbtnid), only if the underlying system does provide guidance there.
  int (*list_buttons)(
    struct pblrt_input *driver,
    int devid,
    int (*cb)(int btnid,int pblbtnid,int hidusage,int lo,int hi,int rest,void *userdata),
    void *userdata
  );
};

#endif
