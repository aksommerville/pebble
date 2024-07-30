#include "alsafd_internal.h"

/* I/O thread.
 */
 
static void *alsafd_iothd(void *arg) {
  struct pblrt_audio *driver=arg;
  while (1) {
    pthread_testcancel();
    
    if (pthread_mutex_lock(&DRIVER->iomtx)) {
      usleep(1000);
      continue;
    }
    if (driver->playing) {
      driver->delegate.cb_pcm(DRIVER->buf,DRIVER->bufa);
    } else {
      memset(DRIVER->buf,0,DRIVER->bufa<<1);
    }
    pthread_mutex_unlock(&DRIVER->iomtx);
    DRIVER->buffer_time_us=alsafd_now();
    
    const uint8_t *src=(uint8_t*)DRIVER->buf;
    int srcc=DRIVER->bufa<<1; // bytes (from samples)
    int srcp=0;
    while (srcp<srcc) {
      pthread_testcancel();
      int pvcancel;
      pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&pvcancel);
      int err=write(DRIVER->fd,src+srcp,srcc-srcp);
      //fprintf(stderr,"alsa write %d/%d: %d\n",srcp,srcc,err);
      pthread_setcancelstate(pvcancel,0);
      if (err<0) {
        if (errno==EPIPE) {
          if (
            (ioctl(DRIVER->fd,SNDRV_PCM_IOCTL_DROP)<0)||
            (ioctl(DRIVER->fd,SNDRV_PCM_IOCTL_DRAIN)<0)||
            (ioctl(DRIVER->fd,SNDRV_PCM_IOCTL_PREPARE)<0)
          ) {
            alsafd_error(driver,"write","Failed to recover from underrun: %m");
            DRIVER->ioerror=-1;
            return 0;
          }
          alsafd_error(driver,"io","Recovered from underrun");
        } else {
          alsafd_error(driver,"write",0);
          DRIVER->ioerror=-1;
          return 0;
        }
      } else {
        srcp+=err;
      }
    }
  }
}

/* Delete context.
 */
 
static void alsafd_del(struct pblrt_audio *driver) {
  
  if (DRIVER->iothd) {
    pthread_cancel(DRIVER->iothd);
    pthread_join(DRIVER->iothd,0);
  }
  
  if (DRIVER->fd>=0) close(DRIVER->fd);
  if (DRIVER->device) free(DRIVER->device);
  if (DRIVER->buf) free(DRIVER->buf);
}

/* Init: Populate (DRIVER->device) with path to the device file.
 * Chooses a device if necessary.
 */
 
static int alsafd_select_device_path(
  struct pblrt_audio *driver,
  const struct pblrt_audio_setup *setup
) {
  if (DRIVER->device) return 0;
  
  // If caller supplied a device path or basename, that trumps all.
  if (setup&&setup->device&&setup->device[0]) {
    if (setup->device[0]=='/') {
      if (!(DRIVER->device=strdup(setup->device))) return -1;
      return 0;
    }
    char tmp[1024];
    int tmpc=snprintf(tmp,sizeof(tmp),"/dev/snd/%s",setup->device);
    if ((tmpc<1)||(tmpc>=sizeof(tmp))) return -1;
    if (!(DRIVER->device=strdup(tmp))) return -1;
    return 0;
  }
  
  // Use some sensible default (rate,chanc) for searching, unless the caller specifies.
  int ratelo=22050,ratehi=48000;
  int chanclo=1,chanchi=2;
  if (setup) {
    if (setup->rate>0) ratelo=ratehi=setup->rate;
    if (setup->chanc>0) chanclo=chanchi=setup->chanc;
  }
  
  // Searching explicitly in "/dev/snd" forces return of an absolute path.
  // If we passed null instead, we'd get the same result, but the basename only.
  if (!(DRIVER->device=alsafd_find_device("/dev/snd",ratelo,ratehi,chanclo,chanchi))) {
    // If it failed with the exact setup params, that's ok, try again with the default ranges.
    if (setup) {
      if (DRIVER->device=alsafd_find_device("/dev/snd",22050,48000,1,2)) return 0;
    }
    return -1;
  }
  return 0;
}

/* Init: Open the device file. (DRIVER->device) must be set.
 */
 
static int alsafd_open_device(struct pblrt_audio *driver) {
  if (DRIVER->fd>=0) return 0;
  if (!DRIVER->device) return -1;
  
  if ((DRIVER->fd=open(DRIVER->device,O_WRONLY))<0) {
    return alsafd_error(driver,"open",0);
  }
  
  // Request ALSA version, really just confirming that the ioctl works, ie it's an ALSA PCM device.
  if (ioctl(DRIVER->fd,SNDRV_PCM_IOCTL_PVERSION,&DRIVER->protocol_version)<0) {
    alsafd_error(driver,"SNDRV_PCM_IOCTL_PVERSION",0);
    close(DRIVER->fd);
    DRIVER->fd=-1;
    return -1;
  }
              
  return 0;
}

/* Init: With device open, send the handshake ioctls to configure it.
 */
 
static int alsafd_configure_device(
  struct pblrt_audio *driver,
  const struct pblrt_audio_setup *setup
) {

  /* Refine hw params against the broadest set of criteria, anything we can technically handle.
   * (we impose a hard requirement for s16 interleaved; that's about it).
   */
  struct snd_pcm_hw_params hwparams;
  alsafd_hw_params_none(&hwparams);
  hwparams.flags=SNDRV_PCM_HW_PARAMS_NORESAMPLE;
  alsafd_hw_params_set_mask(&hwparams,SNDRV_PCM_HW_PARAM_ACCESS,SNDRV_PCM_ACCESS_RW_INTERLEAVED,1);
  alsafd_hw_params_set_mask(&hwparams,SNDRV_PCM_HW_PARAM_FORMAT,SNDRV_PCM_FORMAT_S16,1);
  alsafd_hw_params_set_mask(&hwparams,SNDRV_PCM_HW_PARAM_SUBFORMAT,SNDRV_PCM_SUBFORMAT_STD,1);
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_SAMPLE_BITS,16,16);
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_FRAME_BITS,0,UINT_MAX);
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_CHANNELS,ALSAFD_CHANC_MIN,ALSAFD_CHANC_MAX);
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_RATE,ALSAFD_RATE_MIN,ALSAFD_RATE_MAX);
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_PERIOD_TIME,0,UINT_MAX); // us between interrupts
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_PERIOD_SIZE,0,UINT_MAX); // frames between interrupts
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_PERIOD_BYTES,0,UINT_MAX); // bytes between interrupts
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_PERIODS,0,UINT_MAX); // interrupts per buffer
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_BUFFER_TIME,0,UINT_MAX); // us
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_BUFFER_SIZE,ALSAFD_BUF_MIN,ALSAFD_BUF_MAX); // frames
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_BUFFER_BYTES,0,UINT_MAX);
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_TICK_TIME,0,UINT_MAX); // us
              
  if (ioctl(DRIVER->fd,SNDRV_PCM_IOCTL_HW_REFINE,&hwparams)<0) {
    return alsafd_error(driver,"SNDRV_PCM_IOCTL_HW_REFINE",0);
  }

  if (setup) {
    if (setup->rate>0) alsafd_hw_params_set_nearest_interval(&hwparams,SNDRV_PCM_HW_PARAM_RATE,setup->rate);
    if (setup->chanc>0) alsafd_hw_params_set_nearest_interval(&hwparams,SNDRV_PCM_HW_PARAM_CHANNELS,setup->chanc);
    //if (setup->buffer>0) alsafd_hw_params_set_nearest_interval(&hwparams,SNDRV_PCM_HW_PARAM_BUFFER_SIZE,setup->buffer);
  }
  
  /* Default buffer size to something shorter than s/60.
   * We need very small buffers because Pebble's upper layers are introducing some latency of their own.
   * At 44100 Hz, 512 frames yields about 86 Hz. Unfortunately, we don't usually know the final rate yet.
   */
  if (!setup||(setup->buffer<1)) {
    int bufsize=1024;
    alsafd_hw_params_set_nearest_interval(&hwparams,SNDRV_PCM_HW_PARAM_BUFFER_SIZE,bufsize);
  }

  if (ioctl(DRIVER->fd,SNDRV_PCM_IOCTL_HW_PARAMS,&hwparams)<0) {
    return alsafd_error(driver,"SNDRV_PCM_IOCTL_HW_PARAMS",0);
  }

  if (ioctl(DRIVER->fd,SNDRV_PCM_IOCTL_PREPARE)<0) {
    return alsafd_error(driver,"SNDRV_PCM_IOCTL_PREPARE",0);
  }
  
  // Read the final agreed values off hwparams.
  if (!hwparams.rate_den) return -1;
  driver->rate=hwparams.rate_num/hwparams.rate_den;
  if (alsafd_hw_params_assert_exact_interval(&driver->chanc,&hwparams,SNDRV_PCM_HW_PARAM_CHANNELS)<0) return -1;
  if (alsafd_hw_params_assert_exact_interval(&DRIVER->hwbufframec,&hwparams,SNDRV_PCM_HW_PARAM_BUFFER_SIZE)<0) return -1;
  
  // Validate.
  if ((driver->rate<ALSAFD_RATE_MIN)||(driver->rate>ALSAFD_RATE_MAX)) {
    return alsafd_error(driver,"","Rejecting rate %d, limit %d..%d.",driver->rate,ALSAFD_RATE_MIN,ALSAFD_RATE_MAX);
  }
  if ((driver->chanc<ALSAFD_CHANC_MIN)||(driver->chanc>ALSAFD_CHANC_MAX)) {
    return alsafd_error(driver,"","Rejecting chanc %d, limit %d..%d",driver->chanc,ALSAFD_CHANC_MIN,ALSAFD_CHANC_MAX);
  }
  if ((DRIVER->hwbufframec<ALSAFD_BUF_MIN)||(DRIVER->hwbufframec>ALSAFD_BUF_MAX)) {
    return alsafd_error(driver,"","Rejecting buffer size %d, limit %d..%d",DRIVER->hwbufframec,ALSAFD_BUF_MIN,ALSAFD_BUF_MAX);
  }
  
  DRIVER->bufa=(DRIVER->hwbufframec*driver->chanc)>>1;
  if (DRIVER->buf) free(DRIVER->buf);
  if (!(DRIVER->buf=malloc(DRIVER->bufa<<1))) return -1;
  DRIVER->buftime_s=(double)DRIVER->hwbufframec/(double)driver->rate;
  
  /* Now set some driver software parameters.
   * The main thing is we want avail_min to be half of the hardware buffer size.
   * We will send half-hardware-buffers at a time, and this arrangement should click nicely, let us sleep as much as possible.
   * (in limited experimentation so far, I have found this to be so, and it makes a big impact on overall performance).
   * I've heard that swparams can be used to automatically recover from xrun, but haven't seen that work yet. Not trying here.
   */
  struct snd_pcm_sw_params swparams={
    .tstamp_mode=SNDRV_PCM_TSTAMP_NONE,
    .sleep_min=0,
    .avail_min=DRIVER->hwbufframec>>1,
    .xfer_align=1,
    .start_threshold=0,
    .stop_threshold=DRIVER->hwbufframec,
    .silence_threshold=DRIVER->hwbufframec,
    .silence_size=0,
    .boundary=DRIVER->hwbufframec,
    .proto=DRIVER->protocol_version,
    .tstamp_type=SNDRV_PCM_TSTAMP_NONE,
  };
  if (ioctl(DRIVER->fd,SNDRV_PCM_IOCTL_SW_PARAMS,&swparams)<0) {
    return alsafd_error(driver,"SNDRV_PCM_IOCTL_SW_PARAMS",0);
  }
  
  /* And finally, reset the driver and confirm that it enters PREPARED state.
   */
  if (ioctl(DRIVER->fd,SNDRV_PCM_IOCTL_RESET)<0) {
    return alsafd_error(driver,"SNDRV_PCM_IOCTL_RESET",0);
  }
  struct snd_pcm_status status={0};
  if (ioctl(DRIVER->fd,SNDRV_PCM_IOCTL_STATUS,&status)<0) {
    return alsafd_error(driver,"SNDRV_PCM_IOCTL_STATUS",0);
  }
  if (status.state!=SNDRV_PCM_STATE_PREPARED) {
    return alsafd_error(driver,"","State not PREPARED after setup. state=%d",status.state);
  }
  
  return 0;
}

/* Init: Open and prepare the device.
 */
 
static int alsafd_init_alsa(
  struct pblrt_audio *driver,
  const struct pblrt_audio_setup *setup
) {
  if (alsafd_select_device_path(driver,setup)<0) return -1;
  if (alsafd_open_device(driver)<0) return -1;
  if (alsafd_configure_device(driver,setup)<0) return -1;
  return 0;
}

/* Prepare mutex and thread.
 */
 
static int alsafd_init_thread(struct pblrt_audio *driver) {
  pthread_mutexattr_t mattr;
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr,PTHREAD_MUTEX_RECURSIVE);
  if (pthread_mutex_init(&DRIVER->iomtx,&mattr)) return -1;
  pthread_mutexattr_destroy(&mattr);
  if (pthread_create(&DRIVER->iothd,0,alsafd_iothd,driver)) return -1;
  return 0;
}

/* Init.
 */

static int alsafd_init(struct pblrt_audio *driver,const struct pblrt_audio_setup *setup) {
  DRIVER->fd=-1;
  if (alsafd_init_alsa(driver,setup)<0) return -1;
  if (alsafd_init_thread(driver)<0) return -1;
  return 0;
}

/* Minor operations.
 */

static void alsafd_play(struct pblrt_audio *driver,int play) {
  if (!(driver->playing=play?1:0)) {
    // We must ensure that the callback is not running after a stop.
    if (!pthread_mutex_lock(&DRIVER->iomtx)) pthread_mutex_unlock(&DRIVER->iomtx);
  }
}
 
static int alsafd_lock(struct pblrt_audio *driver) {
  if (pthread_mutex_lock(&DRIVER->iomtx)) return -1;
  return 0;
}

static void alsafd_unlock(struct pblrt_audio *driver) {
  pthread_mutex_unlock(&DRIVER->iomtx);
}

/* Log errors.
 */
 
int alsafd_error(struct pblrt_audio *driver,const char *context,const char *fmt,...) {
  const int log_enabled=1;
  if (log_enabled) {
    if (!context) context="";
    fprintf(stderr,"%s:%s: ",DRIVER->device?DRIVER->device:"alsafd",context);
    if (fmt&&fmt[0]) {
      va_list vargs;
      va_start(vargs,fmt);
      vfprintf(stderr,fmt,vargs);
      fprintf(stderr,"\n");
    } else {
      fprintf(stderr,"%m\n");
    }
  }
  return -1;
}

/* Current time.
 */
 
int64_t alsafd_now() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (int64_t)tv.tv_sec*1000000ll+tv.tv_usec;
}

/* Estimate remaining buffer. TODO I'm sure we'll want something like this, but it's not wired up yet.
 */
 
double alsafd_estimate_remaining_buffer(const struct pblrt_audio *driver) {
  int64_t now=alsafd_now();
  double elapsed=(now-DRIVER->buffer_time_us)/1000000.0;
  if (elapsed<0.0) return 0.0;
  if (elapsed>DRIVER->buftime_s) return DRIVER->buftime_s;
  return DRIVER->buftime_s-elapsed;
}

/* Type definition.
 */
 
const struct pblrt_audio_type pblrt_audio_type_alsafd={
  .name="alsafd",
  .desc="Direct access to ALSA for Linux. Prefer pulse for desktops.",
  .objlen=sizeof(struct pblrt_audio_alsafd),
  .del=alsafd_del,
  .init=alsafd_init,
  .play=alsafd_play,
  .lock=alsafd_lock,
  .unlock=alsafd_unlock,
};
