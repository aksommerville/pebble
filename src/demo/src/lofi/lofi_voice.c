#include "lofi_internal.h"

/* Update envelope.
 */
 
static inline uint8_t lofi_env_next(struct lofi_env *env) {
  uint8_t level;
  if (env->a==env->z) level=env->a;
  else level=(env->a*(env->c-env->p)+env->z*env->p)/env->c;
  if (++(env->p)>=env->c) {
    env->stage++;
    env->p=0;
    switch (env->stage) {
      case 2: env->a=env->atkv; env->z=env->susv; env->c=env->dect; break;
      case 3: env->a=env->susv; env->z=env->susv; env->c=env->sust; break;
      case 4: env->a=env->susv; env->z=0; env->c=env->rlst; break;
      default: env->stage=0; env->c=INT_MAX; env->a=env->z=0; break;
    }
  }
  return level;
}

/* Update voice.
 */
 
void lofi_voice_update(int16_t *v,int c,struct lofi_voice *voice) {
  int updc=voice->ttl;
  if (updc>c) updc=c;
  voice->ttl-=updc;
  const int16_t *src=voice->wave->v;
  for (;updc-->0;v++) {
    uint8_t level=lofi_env_next(&voice->env);
    (*v)+=(src[voice->p>>LOFI_WAVE_SHIFT]*level)>>8;
    voice->p+=voice->dp;
    voice->dp+=voice->ddp;
  }
}

/* Begin note.
 */
 
void lofi_note(uint8_t program,uint8_t trim,uint8_t noteida,uint8_t noteidz,int durms) {
  if (!lofi.rate) return;
  if (noteida>=0x80) return;
  if (noteidz>=0x80) return;
  if (durms<1) return;
  if (durms>LOFI_DUR_LIMIT_MS) durms=LOFI_DUR_LIMIT_MS;
  int envid=program&0xf8;
  int waveid=program&0x07;
  
  struct lofi_voice *voice=0;
  if (lofi.voicec<LOFI_VOICE_LIMIT) {
    voice=lofi.voicev+lofi.voicec++;
  } else {
    struct lofi_voice *q=lofi.voicev;
    voice=q;
    int i=LOFI_VOICE_LIMIT;
    for (;i-->0;q++) {
      if (q->ttl<=voice->ttl) {
        voice=q;
        if (!voice->ttl) break;
      }
    }
  }
  
  voice->wave=lofi.wavev+waveid;
  voice->p=0;
  voice->dp=lofi.step_by_noteid[noteida];
  
  /* From the 5 bits of (envid), populate voice->env.(atkv,susv,atkt,dect,rlst).
   * Levels are normal, we'll do trim next.
   * Times are in ms.
   * - 80: Attack contrast. 
   * - 60: Attack/decay time.
   * - 18: Release time.
   */
  if (envid&0x80) {
    voice->env.atkv=0x60;
    voice->env.susv=0x20;
  } else {
    voice->env.atkv=0x40;
    voice->env.susv=0x30;
  }
  switch (envid&0x60) {
    case 0x00: voice->env.atkt=10; voice->env.dect=20; break;
    case 0x20: voice->env.atkt=20; voice->env.dect=30; break;
    case 0x40: voice->env.atkt=40; voice->env.dect=50; break;
    case 0x60: voice->env.atkt=70; voice->env.dect=70; break;
  }
  switch (envid&0x18) {
    case 0x00: voice->env.rlst= 80; break;
    case 0x08: voice->env.rlst=160; break;
    case 0x10: voice->env.rlst=300; break;
    case 0x18: voice->env.rlst=500; break;
  }
  
  // Apply trim and convert ms to frames. Final envelope.
  voice->env.atkv=(voice->env.atkv*trim)>>8;
  voice->env.susv=(voice->env.susv*trim)>>8;
  if ((voice->env.atkt=(voice->env.atkt*lofi.rate)/1000)<1) voice->env.atkt=1;
  if ((voice->env.dect=(voice->env.dect*lofi.rate)/1000)<1) voice->env.dect=1;
  if ((voice->env.sust=(          durms*lofi.rate)/1000)<1) voice->env.sust=1;
  if ((voice->env.rlst=(voice->env.rlst*lofi.rate)/1000)<1) voice->env.rlst=1;
  voice->env.a=0;
  voice->env.z=voice->env.atkv;
  voice->env.p=0;
  voice->env.c=voice->env.atkt;
  voice->env.stage=1;
  voice->ttl=voice->env.atkt+voice->env.dect+voice->env.sust+voice->env.rlst;
  
  if (noteida==noteidz) voice->ddp=0;
  else voice->ddp=(int32_t)(((double)lofi.step_by_noteid[noteidz]-(double)voice->dp)/(double)voice->ttl);
}
