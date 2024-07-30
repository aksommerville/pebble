/* pblrt_input_type_dummy.c
 * Example input driver.
 * It's only an example. No need for a dummy driver, leave it unset instead.
 * And if we do headless automation in the future, it will patch in further downstream, after mapping.
 */
 
#include "pblrt_drivers.h"

/* Object definition.
 */
 
struct pblrt_input_dummy {
  struct pblrt_input hdr;
};

#define DRIVER ((struct pblrt_input_dummy*)driver)

/* Cleanup.
 */
 
static void _dummy_del(struct pblrt_input *driver) {
  // Clean up everything. Called even if init fails.
  // Do not report the remaining devices as disconnected, just drop them.
}

/* Init.
 */
 
static int _dummy_init(struct pblrt_input *driver,const struct pblrt_input_setup *setup) {
  // Initialize everything.
  // Do not scan for devices yet. Wait for the first update.
  return 0;
}

/* Update.
 */
 
static int _dummy_update() {
  // Pump your event queue and trigger callbacks as warranted.
  return 0;
}

/* List devices.
 */
 
static int _dummy_devid_by_index(struct pblrt_input *input,int p) {
  // You must assign a unique positive "devid" to each connected device, and it must remain constant as long as the device is connected.
  // At any time, your owner may call this to enumerate all connected devices.
  // (p) from zero.
  return -1;
}

/* Get IDs for one device.
 */
 
static const char *_dummy_ids_by_devid(int *vid,int *pid,int *version,struct pblrt_input *input,int devid) {
  // Populate the USB (vid,pid,version) if you know them.
  // Return a constant nul-terminated UTF-8 string if you can arrange that.
  // It's normal to collect this info during connection and cache it on your device object.
  return 0;
}

/* List buttons on one device.
 */
 
static int _dummy_list_buttons(
  struct pblrt_input *input,
  int devid,
  int (*cb)(int btnid,int pblbtnid,int hidusage,int lo,int hi,int rest,void *userdata),
  void *userdata
) {
  // Call (cb) for each button on this device. If it returns nonzero, you must stop iteration and return the same.
  // It's normal to perform expensive I/O during this call. Callers will try to call just once per device.
  // If the underlying system provides physical context for a button, map it to (pblbtnid), if it makes sense.
  // Fine to report zero there if unknown.
  return 0;
}

/* Type definition.
 */
 
const struct pblrt_input_type pblrt_input_type_dummy={
  .name="dummy",
  .desc="Fake input driver, only an example.",
  .objlen=sizeof(struct pblrt_input_dummy),
  .appointment_only=1, // Zero for normal drivers.
  .del=_dummy_del,
  .init=_dummy_init,
  .update=_dummy_update,
  .devid_by_index=_dummy_devid_by_index,
  .ids_by_devid=_dummy_ids_by_devid,
  .list_buttons=_dummy_list_buttons,
};
