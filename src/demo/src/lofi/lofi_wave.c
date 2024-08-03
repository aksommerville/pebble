#include "lofi_internal.h"

/* Require private sine reference.
 */
 
static void lofi_wave_require_sine() {
  int16_t *v=lofi.sine.v;
  if (v[LOFI_WAVE_SIZE_SAMPLES>>2]) return; // [0] should be 0, check about a quarter-turn in.
  int i=LOFI_WAVE_SIZE_SAMPLES;
  double p=0.0,dp=(M_PI*2.0)/LOFI_WAVE_SIZE_SAMPLES;
  for (;i-->0;v++,p+=dp) {
    *v=(int16_t)(sin(p)*32767.0);
  }
}

/* Init sinee.
 */
 
int lofi_wave_init_sine(int waveid) {
  if ((waveid<0)||(waveid>=LOFI_WAVE_LIMIT)) return -1;
  struct lofi_wave *wave=lofi.wavev+waveid;
  lofi_wave_require_sine();
  memcpy(wave->v,lofi.sine.v,sizeof(lofi.sine.v));
  return 0;
}

/* Init square.
 */
 
int lofi_wave_init_square(int waveid) {
  if ((waveid<0)||(waveid>=LOFI_WAVE_LIMIT)) return -1;
  struct lofi_wave *wave=lofi.wavev+waveid;
  int halflen=LOFI_WAVE_SIZE_SAMPLES>>1;
  int16_t *v=wave->v;
  int i;
  for (i=halflen;i-->0;v++) *v=32767;
  for (i=halflen;i-->0;v++) *v=-32768;
  return 0;
}

/* Init saw.
 */
 
int lofi_wave_init_saw(int waveid) {
  if ((waveid<0)||(waveid>=LOFI_WAVE_LIMIT)) return -1;
  struct lofi_wave *wave=lofi.wavev+waveid;
  int16_t step=65535/LOFI_WAVE_SIZE_SAMPLES;
  int16_t p=-32768;
  int16_t *v=wave->v;
  int i=LOFI_WAVE_SIZE_SAMPLES;
  for (;i-->0;v++,p+=step) *v=p;
  return 0;
}

/* Init triangle.
 */
 
int lofi_wave_init_triangle(int waveid) {
  if ((waveid<0)||(waveid>=LOFI_WAVE_LIMIT)) return -1;
  struct lofi_wave *wave=lofi.wavev+waveid;
  int16_t step=131072/LOFI_WAVE_SIZE_SAMPLES;
  int16_t p=-32768;
  int16_t *v=wave->v;
  int16_t *back=wave->v+LOFI_WAVE_SIZE_SAMPLES;
  int halflen=LOFI_WAVE_SIZE_SAMPLES>>1,i;
  for (i=halflen;i-->0;v++,p+=step) {
    *v=p;
    back--;
    *back=p;
  }
  return 0;
}

/* Init harmonics.
 */
 
static void lofi_wave_copy_harmonic(int16_t *dst,uint8_t trim,int step) {
  if (step>=LOFI_WAVE_SIZE_SAMPLES) return;
  int srcp=0,i=LOFI_WAVE_SIZE_SAMPLES;
  for (;i-->0;dst++) {
    int wide=(*dst)+((lofi.sine.v[srcp]*trim)>>8);
    if (wide<-32768) *dst=-32768;
    else if (wide>32767) *dst=32767;
    else *dst=wide;
    if ((srcp+=step)>=LOFI_WAVE_SIZE_SAMPLES) srcp-=LOFI_WAVE_SIZE_SAMPLES;
  }
}
 
int lofi_wave_init_harmonics(int waveid,const uint8_t *coefv,int coefc) {
  if ((waveid<0)||(waveid>=LOFI_WAVE_LIMIT)) return -1;
  lofi_wave_require_sine();
  struct lofi_wave *wave=lofi.wavev+waveid;
  int16_t *v=wave->v;
  memset(v,0,sizeof(wave->v));
  int i=0;
  for (;i<coefc;i++) {
    if (coefv[i]) lofi_wave_copy_harmonic(v,coefv[i],i+1);
  }
  return 0;
}
