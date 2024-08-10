#include "pulse_internal.h"

/* I/O thread.
 */
 
static void *pulse_iothd(void *arg) {
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
    DRIVER->buffer_time_us=pulse_now();
    
    int err=0,result;
    pthread_testcancel();
    int pvcancel;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&pvcancel);
    result=pa_simple_write(DRIVER->pa,DRIVER->buf,sizeof(int16_t)*DRIVER->bufa,&err);
    pthread_setcancelstate(pvcancel,0);
    if (result<0) {
      DRIVER->ioerror=-1;
      return 0;
    }
  }
}

/* Delete.
 */

static void _pulse_del(struct pblrt_audio *driver) {
  if (DRIVER->iothd) {
    pthread_cancel(DRIVER->iothd);
    pthread_join(DRIVER->iothd,0);
  }
  if (DRIVER->pa) pa_simple_free(DRIVER->pa);
  if (DRIVER->buf) free(DRIVER->buf);
}

/* Init PulseAudio client.
 */
 
static int pulse_init_pa(struct pblrt_audio *driver,const struct pblrt_audio_setup *setup) {
  int err;
  
  const char *appname="Pulse Client";
  const char *servername=0;
  int buffersize=0;
  if (setup) {
    if (setup->rate>0) driver->rate=setup->rate;
    if (setup->chanc>0) driver->chanc=setup->chanc;
    if (setup->buffer>0) buffersize=setup->buffer;
  }
  if (driver->rate<1) driver->rate=44100;
  if (driver->chanc<1) driver->chanc=2;
  if (buffersize<1) buffersize=driver->rate/20;
  if (buffersize<20) buffersize=20;

  pa_sample_spec sample_spec={
    #if BYTE_ORDER==BIG_ENDIAN
      .format=PA_SAMPLE_S16BE,
    #else
      .format=PA_SAMPLE_S16LE,
    #endif
    .rate=driver->rate,
    .channels=driver->chanc,
  };
  pa_buffer_attr buffer_attr={
    .maxlength=driver->chanc*sizeof(int16_t)*buffersize,
    .tlength=driver->chanc*sizeof(int16_t)*buffersize,
    .prebuf=0xffffffff,
    .minreq=0xffffffff,
  };
  
  if (!(DRIVER->pa=pa_simple_new(
    servername,
    appname,
    PA_STREAM_PLAYBACK,
    0, // sink name (?)
    appname,
    &sample_spec,
    0, // channel map
    &buffer_attr,
    &err
  ))) {
    return -1;
  }
  
  driver->rate=sample_spec.rate;
  driver->chanc=sample_spec.channels;
  
  return 0;
}

/* With the final rate and channel count settled, calculate a good buffer size and allocate it.
 */
 
static int pulse_init_buffer(struct pblrt_audio *driver,const struct pblrt_audio_setup *setup) {

  const double buflen_target_s= 0.010; // about 100 Hz
  const int buflen_min=           128; // but in no case smaller than N samples
  const int buflen_max=         16384; // ...nor larger
  
  // Initial guess and clamp to the hard boundaries.
  if (setup->buffer>0) DRIVER->bufa=setup->buffer;
  else DRIVER->bufa=buflen_target_s*driver->rate*driver->chanc;
  if (DRIVER->bufa<buflen_min) {
    DRIVER->bufa=buflen_min;
  } else if (DRIVER->bufa>buflen_max) {
    DRIVER->bufa=buflen_max;
  }
  // Reduce to next multiple of channel count.
  DRIVER->bufa-=DRIVER->bufa%driver->chanc;
  
  if (!(DRIVER->buf=malloc(sizeof(int16_t)*DRIVER->bufa))) {
    return -1;
  }
  DRIVER->buftime_s=(double)(DRIVER->bufa/driver->chanc)/(double)driver->rate;
  
  return 0;
}

/* Prepare mutex and thread.
 */
 
static int pulse_init_thread(struct pblrt_audio *driver) {
  pthread_mutexattr_t mattr;
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr,PTHREAD_MUTEX_RECURSIVE);
  if (pthread_mutex_init(&DRIVER->iomtx,&mattr)) return -1;
  pthread_mutexattr_destroy(&mattr);
  if (pthread_create(&DRIVER->iothd,0,pulse_iothd,driver)) return -1;
  return 0;
}

/* New.
 */

static int _pulse_init(struct pblrt_audio *driver,const struct pblrt_audio_setup *setup) {
  if (pulse_init_pa(driver,setup)<0) return -1;
  if (pulse_init_buffer(driver,setup)<0) return -1;
  if (pulse_init_thread(driver)<0) return -1;
  return 0;
}

/* Play.
 */

static void _pulse_play(struct pblrt_audio *driver,int play) {
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

/* Update.
 */

static int _pulse_update(struct pblrt_audio *driver) {
  if (DRIVER->ioerror) return -1;
  return 0;
}

/* Lock.
 */
 
static int _pulse_lock(struct pblrt_audio *driver) {
  if (pthread_mutex_lock(&DRIVER->iomtx)) return -1;
  return 0;
}

static void _pulse_unlock(struct pblrt_audio *driver) {
  pthread_mutex_unlock(&DRIVER->iomtx);
}

/* Current time.
 */
 
int64_t pulse_now() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (int64_t)tv.tv_sec*1000000ll+tv.tv_usec;
}

/* Estimate remaining buffer.
 */
 
double pulse_estimate_remaining_buffer(const struct pblrt_audio *driver) {
  int64_t now=pulse_now();
  double elapsed=(now-DRIVER->buffer_time_us)/1000000.0;
  if (elapsed<0.0) return 0.0;
  if (elapsed>DRIVER->buftime_s) return DRIVER->buftime_s;
  return DRIVER->buftime_s-elapsed;
}

/* Type definition.
 */
 
const struct pblrt_audio_type pblrt_audio_type_pulse={
  .name="pulse",
  .desc="Linux audio via PulseAudio. Preferred for most desktops.",
  .objlen=sizeof(struct pblrt_audio_pulse),
  .del=_pulse_del,
  .init=_pulse_init,
  .play=_pulse_play,
  .update=_pulse_update,
  .lock=_pulse_lock,
  .unlock=_pulse_unlock,
};
