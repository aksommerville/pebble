#ifndef PULSE_INTERNAL_H
#define PULSE_INTERNAL_H

#include "pblrt/pblrt_drivers.h"
#include <stdlib.h>
#include <string.h>
#include <endian.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>

struct pblrt_audio_pulse {
  struct pblrt_audio hdr;
  pthread_t iothd;
  pthread_mutex_t iomtx;
  int ioerror;
  int16_t *buf;
  int bufa; // samples
  pa_simple *pa;
  int64_t buffer_time_us;
  double buftime_s;
};

#define DRIVER ((struct pblrt_audio_pulse*)driver)

int64_t pulse_now();

#endif
