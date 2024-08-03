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
  }
}

/* Begin note.
 */
 
void lofi_note(int waveid,int trim,int noteid,int durms) {
  if (!lofi.rate) return;
  if ((waveid<0)||(waveid>=LOFI_WAVE_LIMIT)) return;
  if ((noteid<0)||(noteid>=0x80)) return;
  if (durms<1) return;
  if (durms>LOFI_DUR_LIMIT_MS) durms=LOFI_DUR_LIMIT_MS;
  //pbl_log("%s(%d,%d,%d,%d)",__func__,waveid,trim,noteid,durms);
  
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
  voice->dp=lofi.step_by_noteid[noteid];
  
  //TODO Configurable envelope.
  voice->env.atkv=trim>>2;
  voice->env.susv=voice->env.atkv>>2;
  if ((voice->env.atkt=(   15*lofi.rate)/1000)<1) voice->env.atkt=1;
  if ((voice->env.dect=(   20*lofi.rate)/1000)<1) voice->env.dect=1;
  if ((voice->env.sust=(durms*lofi.rate)/1000)<1) voice->env.sust=1;
  if ((voice->env.rlst=(  300*lofi.rate)/1000)<1) voice->env.rlst=1;
  voice->env.a=0;
  voice->env.z=voice->env.atkv;
  voice->env.p=0;
  voice->env.c=voice->env.atkt;
  voice->env.stage=1;
  
  //pbl_log("env timing: %d,%d,%d,%d",voice->env.atkt,voice->env.dect,voice->env.sust,voice->env.rlst);
  
  voice->ttl=voice->env.atkt+voice->env.dect+voice->env.sust+voice->env.rlst;
}
