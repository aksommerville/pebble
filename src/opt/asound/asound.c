#include "asound_internal.h"

/* I/O thread.
 */
 
static void *asound_iothd(void *arg) {
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
    DRIVER->buffer_time_us=asound_now();
    
    int framec=DRIVER->bufa_frames;
    int framep=0;
    while (framep<framec) {
      pthread_testcancel();
      int pvcancel;
      pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&pvcancel);
      int err=snd_pcm_writei(DRIVER->alsa,DRIVER->buf+framep*driver->chanc,framec-framep);
      pthread_setcancelstate(pvcancel,0);
      if (err<=0) {
        if (snd_pcm_recover(DRIVER->alsa,err,0)<0) return 0;
        break;
      }
      framep+=err;
    }
  }
}

/* Delete.
 */

static void _asound_del(struct pblrt_audio *driver) {
  if (DRIVER->iothd) {
    pthread_cancel(DRIVER->iothd);
    pthread_join(DRIVER->iothd,0);
  }
  if (DRIVER->alsa) snd_pcm_close(DRIVER->alsa);
  if (DRIVER->buf) free(DRIVER->buf);
}

/* Init.
 */
 
static int _asound_init(struct pblrt_audio *driver,const struct pblrt_audio_setup *setup) {
  
  const char *device=0;
  if (setup) {
    driver->rate=setup->rate;
    driver->chanc=setup->chanc;
    device=setup->device;
  }
       if (driver->rate<200) driver->rate=44100;
  else if (driver->rate>200000) driver->rate=44100;
       if (driver->chanc<1) driver->chanc=1;
  else if (driver->chanc>8) driver->chanc=8;
  if (!device) device="default";

  DRIVER->bufa_frames=driver->rate/30;

  snd_pcm_hw_params_t *hwparams=0;
  if (
    (snd_pcm_open(&DRIVER->alsa,device,SND_PCM_STREAM_PLAYBACK,0)<0)||
    (snd_pcm_hw_params_malloc(&hwparams)<0)||
    (snd_pcm_hw_params_any(DRIVER->alsa,hwparams)<0)||
    (snd_pcm_hw_params_set_access(DRIVER->alsa,hwparams,SND_PCM_ACCESS_RW_INTERLEAVED)<0)||
    (snd_pcm_hw_params_set_format(DRIVER->alsa,hwparams,SND_PCM_FORMAT_S16)<0)||
    (snd_pcm_hw_params_set_rate_near(DRIVER->alsa,hwparams,&driver->rate,0)<0)||
    (snd_pcm_hw_params_set_channels_near(DRIVER->alsa,hwparams,&driver->chanc)<0)||
    (snd_pcm_hw_params_set_buffer_size_near(DRIVER->alsa,hwparams,&DRIVER->bufa_frames)<0)||
    (snd_pcm_hw_params(DRIVER->alsa,hwparams)<0)
  ) {
    snd_pcm_hw_params_free(hwparams);
    return -1;
  }
  
  snd_pcm_hw_params_free(hwparams);
  
  if (snd_pcm_nonblock(DRIVER->alsa,0)<0) return -1;
  if (snd_pcm_prepare(DRIVER->alsa)<0) return -1;

  DRIVER->bufa=DRIVER->bufa_frames*driver->chanc;
  if (!(DRIVER->buf=malloc(DRIVER->bufa*2))) return -1;
  DRIVER->buftime_s=(double)DRIVER->bufa_frames/(double)driver->rate;

  pthread_mutexattr_t mattr;
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr,PTHREAD_MUTEX_RECURSIVE);
  if (pthread_mutex_init(&DRIVER->iomtx,&mattr)) return -1;
  pthread_mutexattr_destroy(&mattr);
  if (pthread_create(&DRIVER->iothd,0,asound_iothd,driver)) return -1;

  return 0;
}

/* Play/pause.
 */

static void _asound_play(struct pblrt_audio *driver,int play) {
  if (play) {
    if (driver->playing) return;
    driver->playing=1;
  } else {
    if (!driver->playing) return;
    if (pthread_mutex_lock(&DRIVER->iomtx)) return;
    driver->playing=0;
    pthread_mutex_unlock(&DRIVER->iomtx);
  }
}

static int _asound_lock(struct pblrt_audio *driver) {
  if (pthread_mutex_lock(&DRIVER->iomtx)) return -1;
  return 0;
}

static void _asound_unlock(struct pblrt_audio *driver) {
  pthread_mutex_unlock(&DRIVER->iomtx);
}

/* Current time.
 */
 
int64_t asound_now() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (int64_t)tv.tv_sec*1000000ll+tv.tv_usec;
}

/* Estimate remaining buffer.
 */
 
double asound_estimate_remaining_buffer(const struct pblrt_audio *driver) {
  int64_t now=asound_now();
  double elapsed=(now-DRIVER->buffer_time_us)/1000000.0;
  if (elapsed<0.0) return 0.0;
  if (elapsed>DRIVER->buftime_s) return DRIVER->buftime_s;
  return DRIVER->buftime_s-elapsed;
}

/* Type definition.
 */
 
const struct pblrt_audio_type pblrt_audio_type_asound={
  .name="asound",
  .desc="Audio for Linux via libasound (ALSA).",
  .objlen=sizeof(struct pblrt_audio_asound),
  .del=_asound_del,
  .init=_asound_init,
  .play=_asound_play,
  .lock=_asound_lock,
  .unlock=_asound_unlock,
};
