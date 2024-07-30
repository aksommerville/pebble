#include "pbltool_internal.h"

/* tid
 */
 
int pbltool_tid_eval(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!srcc) return 0;
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!memcmp(src,#tag,srcc)) return PBL_TID_##tag;
  PBL_TID_FOR_EACH
  #undef _
  int tid;
  if ((sr_int_eval(&tid,src,srcc)>=2)&&(tid>0)&&(tid<0x40)) return tid;
  return -1;
}

int pbltool_tid_repr(char *dst,int dsta,int tid) {
  if (!dst||(dsta<0)) dsta=0;
  const char *src=0;
  int srcc=0;
  switch (tid) {
    #define _(tag) case PBL_TID_##tag: src=#tag; srcc=sizeof(#tag)-1; break;
    PBL_TID_FOR_EACH
    #undef _
  }
  if (srcc>0) {
    if (srcc<=dsta) {
      memcpy(dst,src,srcc);
      if (srcc<dsta) dst[srcc]=0;
    }
    return srcc;
  }
  return sr_decsint_repr(dst,dsta,tid);
}
