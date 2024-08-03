#include "lofi_internal.h"

/* Init sine.
 */
 
int lofi_wave_init_sine(int waveid) {
  if ((waveid<0)||(waveid>=LOFI_WAVE_LIMIT)) return -1;
  struct lofi_wave *wave=lofi.wavev+waveid;
  int16_t *v=wave->v;
  int i=LOFI_WAVE_SIZE_SAMPLES;
  double p=0.0,dp=(M_PI*2.0)/LOFI_WAVE_SIZE_SAMPLES;
  for (;i-->0;v++,p+=dp) {
    *v=(int16_t)(sin(p)*32767.0);
  }
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
