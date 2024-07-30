#ifndef ALSAFD_INTERNAL_H
#define ALSAFD_INTERNAL_H

#include "pblrt/pblrt_drivers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <pthread.h>

// asound.h uses SNDRV_LITTLE_ENDIAN/SNDRV_BIG_ENDIAN but never defines them.
#include <endian.h>
#if BYTE_ORDER==LITTLE_ENDIAN
  #define SNDRV_LITTLE_ENDIAN 1
#elif BYTE_ORDER==BIG_ENDIAN
  #define SNDRV_BIG_ENDIAN 1
#endif
#include <sound/asound.h>

// Sanity limits we impose artificially. You will never get a context outside these bounds.
#define ALSAFD_RATE_MIN     200
#define ALSAFD_RATE_MAX  200000
#define ALSAFD_CHANC_MIN      1
#define ALSAFD_CHANC_MAX     16
#define ALSAFD_BUF_MIN       64
#define ALSAFD_BUF_MAX    16384

struct pblrt_audio_alsafd {
  struct pblrt_audio hdr;
  int fd;
  int hwbufframec;
  char *device;
  int protocol_version;
  pthread_t iothd;
  pthread_mutex_t iomtx;
  int ioerror;
  int16_t *buf;
  int bufa; // samples
  int64_t buffer_time_us;
  double buftime_s;
};

#define DRIVER ((struct pblrt_audio_alsafd*)driver)

// Log if enabled, and always returns -1. Null (fmt) to use errno.
int alsafd_error(struct pblrt_audio *driver,const char *context,const char *fmt,...);

void alsafd_hw_params_any(struct snd_pcm_hw_params *params);
void alsafd_hw_params_none(struct snd_pcm_hw_params *params);
int alsafd_hw_params_get_mask(const struct snd_pcm_hw_params *params,int k,int bit);
int alsafd_hw_params_get_interval(int *lo,int *hi,const struct snd_pcm_hw_params *params,int k);
int alsafd_hw_params_assert_exact_interval(int *v,const struct snd_pcm_hw_params *params,int k);
void alsafd_hw_params_set_mask(struct snd_pcm_hw_params *params,int k,int bit,int v);
void alsafd_hw_params_set_interval(struct snd_pcm_hw_params *params,int k,int lo,int hi);

// Having already refined the given param, choose a single value for it as close as possible to (v).
void alsafd_hw_params_set_nearest_interval(struct snd_pcm_hw_params *params,int k,unsigned int v);

int64_t alsafd_now();

/* Iterate possible PCM devices in a given directory.
 * We only report devices that support interleaved output of native s16.
 * Stop iteration by returning nonzero; we return the same.
 */
struct alsafd_device {
  const char *basename; // Give this to the ctor to use this device.
  const char *path;
  int protocol_version; // SNDRV_PCM_IOCTL_PVERSION, as reported by device.
  int compiled_version; // SNDRV_PCM_VERSION, what we were compiled against.
  // SNDRV_PCM_IOCTL_INFO:
  int card; // (card,device) are usually part of the basename.
  int device;
  int subdevice;
  const char *id; // I've observed that (id) and (name) are always the same. (with limited testing)
  const char *name;
  const char *subname;
  int dev_class;
  int dev_subclass;
  // SNDRV_PCM_IOCTL_HW_REFINE:
  int ratelo,ratehi;
  int chanclo,chanchi;
};
int alsafd_list_devices(
  const char *path, // null for the default "/dev/snd"
  int (*cb)(struct alsafd_device *device,void *userdata),
  void *userdata
);

/* Convenience, find the device best matching your preferred rate and channel count.
 * We prefer lower device and card numbers, so you'll usually get pcmC0D0p.
 * Whatever we return, you can use as setup->device, and then you must free it.
 */
char *alsafd_find_device(
  const char *path, // null for the default "/dev/snd"
  int ratelo,int ratehi,
  int chanclo,int chanchi
);

#endif
