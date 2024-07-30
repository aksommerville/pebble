/* pblrt_video_type_dummy.c
 * This is an example video implementation.
 * It can also serve for headless automation, if we ever support that generally.
 */
 
#include "pblrt_drivers.h"

/* Object definition.
 */
 
struct pblrt_video_dummy {
  struct pblrt_video hdr;
};

#define DRIVER ((struct pblrt_video_dummy*)driver)

/* Delete.
 */
 
static void _dummy_del(struct pblrt_video *driver) {
  // Clean everything up. This DOES get called if init fails.
}

/* Init.
 */
 
static int _dummy_init(struct pblrt_video *driver,const struct pblrt_video_setup *setup) {
  // (setup) could be straight zeroes. Anything set in there, try to obey it.
  // setup->(w,h) is a concrete request you should try to follow exactly. It's usually zero.
  // setup->(fbw,fbh) is the underlying framebuffer size. If you're guessing, try to preserve this aspect ratio.
  // If you support a system pointer, hide it.
  // Set driver->(w,h) to the real client size before returning.
  // If you only do fullscreen, set (driver->fullscreen) nonzero.
  // Most video drivers will engage some kind of GX for scaling up. That's entirely your responsibility, and setup it up right here.
  return 0;
}

/* Update.
 */
 
static int _dummy_update(struct pblrt_video *driver) {
  // Pump your event bus and trigger delegate callbacks as warranted.
  // If you don't supply events, no need to implement this hook.
  return 0;
}

/* Toggle fullscreen.
 */
 
static void _dummy_set_fullscreen(struct pblrt_video *driver,int fullscreen) {
  // Update (driver->fullscreen) if you change it.
  // If you can only do windowed or only fullscreen, no need to implement this hook.
}

/* Commit framebuffer.
 */
 
static int _dummy_commit_framebuffer(struct pblrt_video *video,const void *rgba,int w,int h) {
  // (rgba) is packed LRTB with Red in the first byte.
  // Do whatever you do to scale up and put it on screen.
  // (w,h) should not change, should always be the (fbw,fbh) reported at init. But don't blindly assume so.
  //TODO If we ever support headless automation, patch in here for screencaps and maybe framebuffer analysis?
  return 0;
}

/* Unusual hooks.
 */
 
static void _dummy_skip(struct pblrt_video *video) {
  // Core calls this in place of commit_framebuffer on frames when the game declined to produce video.
  // Keep the last framebuffer up.
  // I don't think any drivers will need this hook. But maybe you have to do something to update a clock or whatever.
}

static void _dummy_suppress_screensaver(struct pblrt_video *video) {
  // Core calls this whenever there is input from the joystick bus, potentially multiple times per frame.
  // X11 needs it, otherwise joysticks don't keep the screen awake.
}

/* Type definition.
 */
 
const struct pblrt_video_type pblrt_video_type_dummy={
  .name="dummy",
  .desc="Fake video driver that does nothing.",
  .objlen=sizeof(struct pblrt_video_dummy),
  .appointment_only=1, // Zero for real drivers. Nonzero to decline automatic selection.
  .provides_keyboard=0,
  .del=_dummy_del,
  .init=_dummy_init,
  .update=_dummy_update,
  .set_fullscreen=_dummy_set_fullscreen,
  .commit_framebuffer=_dummy_commit_framebuffer,
  .skip=_dummy_skip,
  .suppress_screensaver=_dummy_suppress_screensaver,
};
