#include "pblrt_drivers.h"
#include <string.h>

/* Declare all possible implementations here.
 * Doesn't matter if they don't exist.
 */
extern const struct pblrt_video_type pblrt_video_type_dummy;
extern const struct pblrt_video_type pblrt_video_type_xegl;
extern const struct pblrt_video_type pblrt_video_type_drmgx;
extern const struct pblrt_video_type pblrt_video_type_bcm;
extern const struct pblrt_video_type pblrt_video_type_mswm;//TODO
extern const struct pblrt_video_type pblrt_video_type_macwm;//TODO
extern const struct pblrt_audio_type pblrt_audio_type_dummy;
extern const struct pblrt_audio_type pblrt_audio_type_alsafd;
extern const struct pblrt_audio_type pblrt_audio_type_asound;
extern const struct pblrt_audio_type pblrt_audio_type_pulse;
extern const struct pblrt_audio_type pblrt_audio_type_msaudio;//TODO
extern const struct pblrt_audio_type pblrt_audio_type_macaudio;//TODO
extern const struct pblrt_input_type pblrt_input_type_dummy;
extern const struct pblrt_input_type pblrt_input_type_evdev;
extern const struct pblrt_input_type pblrt_input_type_mshid;//TODO
extern const struct pblrt_input_type pblrt_input_type_machid;//TODO

/* Then, employing preprocessor conditions, compose the list of actually-available drivers, in order of preference.
 * By default, the runtime tries these lists in order.
 */

static const struct pblrt_video_type *pblrt_video_typev[]={
#if USE_xegl
  &pblrt_video_type_xegl,
#endif
#if USE_drmgx
  &pblrt_video_type_drmgx,
#endif
#if USE_bcm
  &pblrt_video_type_bcm,
#endif
  &pblrt_video_type_dummy,
};

static const struct pblrt_audio_type *pblrt_audio_typev[]={
#if USE_pulse
  &pblrt_audio_type_pulse,
#endif
#if USE_asound
  &pblrt_audio_type_asound,
#endif
#if USE_alsafd
  &pblrt_audio_type_alsafd,
#endif
  &pblrt_audio_type_dummy,
};

static const struct pblrt_input_type *pblrt_input_typev[]={
#if USE_evdev
  &pblrt_input_type_evdev,
#endif
  &pblrt_input_type_dummy,
};

/* The rest is automatic.
 */

#define ACCESSORS(intf) \
  const struct pblrt_##intf##_type *pblrt_##intf##_type_by_index(int p) { \
    if (p<0) return 0; \
    int c=sizeof(pblrt_##intf##_typev)/sizeof(void*); \
    if (p>=c) return 0; \
    return pblrt_##intf##_typev[p]; \
  } \
  const struct pblrt_##intf##_type *pblrt_##intf##_type_by_name(const char *name,int namec) { \
    if (!name) return 0; \
    if (namec<0) { namec=0; while (name[namec]) namec++; } \
    const struct pblrt_##intf##_type **p=pblrt_##intf##_typev; \
    int i=sizeof(pblrt_##intf##_typev)/sizeof(void*); \
    for (;i-->0;p++) { \
      const struct pblrt_##intf##_type *type=*p; \
      if (memcmp(type->name,name,namec)) continue; \
      if (type->name[namec]) continue; \
      return type; \
    } \
    return 0; \
  }
  
ACCESSORS(video)
ACCESSORS(audio)
ACCESSORS(input)

#undef ACCESSORS
