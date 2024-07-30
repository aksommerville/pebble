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

/* MIME type.
 */
 
const char *pbltool_guess_mime_type(const void *src,int srcc,const char *path,int pathc) {

  /* If there's a known unambiguous path suffix, case-insensitively, trust it.
   */
  if (path) {
    if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
    int dotp=-1;
    int i=pathc; while (i-->0) {
      if (path[i]=='.') { dotp=i; break; }
      if (path[i]=='/') break;
    }
    if (dotp>=0) {
      int sfxc=pathc-dotp-1;
      char norm[16];
      if ((sfxc>0)&&(sfxc<=sizeof(norm))) {
        const char *sfxsrc=path+dotp+1;
        for (i=sfxc;i-->0;) {
          if ((sfxsrc[i]>='A')&&(sfxsrc[i]<='Z')) norm[i]=sfxsrc[i]+0x20;
          else norm[i]=sfxsrc[i];
        }
        switch (sfxc) {
          case 2: {
              if (!memcmp(norm,"js",2)) return "application/javascript";
            } break;
          case 3: {
              if (!memcmp(norm,"css",3)) return "text/css";
              if (!memcmp(norm,"htm",3)) return "text/html";
              if (!memcmp(norm,"png",3)) return "image/png";
              if (!memcmp(norm,"gif",3)) return "image/gif";
              if (!memcmp(norm,"ico",3)) return "image/x-icon";
              if (!memcmp(norm,"jpg",3)) return "image/jpeg";
              if (!memcmp(norm,"mid",3)) return "audio/midi";
              if (!memcmp(norm,"wav",3)) return "audio/wave";
              if (!memcmp(norm,"pbl",3)) return "application/x-pebble-rom";
            } break;
          case 4: {
              if (!memcmp(norm,"html",4)) return "text/html";
              if (!memcmp(norm,"jpeg",4)) return "image/jpeg";
              if (!memcmp(norm,"json",4)) return "application/json";
            } break;
        }
      }
    }
  }
  
  /* Look for unambiguous signatures.
   */
  if ((srcc>=8)&&!memcmp(src,"\x89PNG\r\n\x1a\n",8)) return "image/png";
  if ((srcc>=8)&&!memcmp(src,"MThd\0\0\0\6",8)) return "audio/midi";
  if ((srcc>=8)&&!memcmp(src,"\x00\xffPBLROM",8)) return "application/x-pebble-rom";
  if ((srcc>=6)&&!memcmp(src,"GIF87a",6)) return "image/gif";
  if ((srcc>=6)&&!memcmp(src,"GIF89a",6)) return "image/gif";
  
  /* "text/plain" or "application/octet-stream", based on the first 256 bytes.
   */
  if (!srcc) return "application/octet-stream";
  const uint8_t *v=src;
  int i=srcc;
  if (i>256) i=256;
  for (;i-->0;v++) {
    if (*v==0x09) continue;
    if (*v==0x0a) continue;
    if (*v==0x0d) continue;
    if (*v<0x20) return "application/octet-stream";
    if (*v>=0x7f) return "application/octet-stream";
  }
  return "text/plain";
}
