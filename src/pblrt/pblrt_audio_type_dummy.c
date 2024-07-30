/* pblrt_audio_type_dummy.c
 * This is only an example template for new audio drivers.
 * Audio driver is always optional, so there's no need for a dummy.
 */
 
#include "pblrt_drivers.h"

/* Object definition.
 */
 
struct pblrt_audio_dummy {
  struct pblrt_audio hdr;
};

#define DRIVER ((struct pblrt_audio_dummy*)driver)

/* Cleanup.
 */
 
static void _dummy_del(struct pblrt_audio *driver) {
  // Clean everything up. Owner will call play(0) before deleting.
}

/* Init.
 */
 
static int _dummy_init(struct pblrt_audio *driver,const struct pblrt_audio_setup *setup) {
  // Try to respect (setup) as much as possible, where it's nonzero.
  // But you are allowed to override the config in any way that makes sense for the hardware.
  // Do not start playing initially.
  driver->rate=44100;
  driver->chanc=2;
  return 0;
}

/* Play/stop.
 */
 
static void _dummy_play(struct pblrt_audio *driver,int play) {
  // Begin or end, and update (driver->playing).
  // When stopping, you must ensure that any callbacks are no longer running before returning.
  // You can do that cheap with a lock/unlock right after toggling your things.
}

/* Update.
 */
 
static int _dummy_update(struct pblrt_audio *driver) {
  // Advance time and call back as needed.
  // Drivers are encouraged to use a background thread for I/O, and should not need this.
  return 0;
}

/* Lock/unlock.
 */
 
static int _dummy_lock(struct pblrt_audio *driver) {
  // When caller holds the lock, you guarantee that his callback is not running, even if it causes you to underflow.
  return 0;
}

static void _dummy_unlock(struct pblrt_audio *driver) {
}

/* Type definition.
 */
 
const struct pblrt_audio_type pblrt_audio_type_dummy={
  .name="dummy",
  .desc="Fake audio driver that does nothing.",
  .objlen=sizeof(struct pblrt_audio_dummy),
  .appointment_only=1, // Usually zero. Nonzero to exclude from automatic selection.
  .del=_dummy_del,
  .init=_dummy_init,
  .play=_dummy_play,
  .update=_dummy_update,
  .lock=_dummy_lock,
  .unlock=_dummy_unlock,
};
