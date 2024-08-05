#include "pblrt_internal.h"
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

void pbl_log(const char *fmt,...) {
  // pbl_log() is a subset of printf(), so just use real printf since we have it.
  va_list vargs;
  va_start(vargs,fmt);
  char msg[1024];
  int msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
  if ((msgc<1)||(msgc>=sizeof(msg))) msgc=0;
  fprintf(stderr,"GAME: %.*s\n",msgc,msg);
}

void pbl_terminate(int status) {
  pblrt.terminate++;
  pblrt.termstatus=(status<0)?1:status;
}

void pbl_set_synth_limit(int samplec) {
  pblrt.synth_limit=samplec;
}

double pbl_now_real() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (double)tv.tv_sec+(double)tv.tv_usec/1000000.0;
}

void pbl_now_local(int *dst,int dsta) {
  if (!dst||(dsta<1)) return;
  time_t now=time(0);
  struct tm *tm=localtime(&now);
  if (!tm) return;
  if (dsta>7) dsta=7;
  *dst++=1900+tm->tm_year; if (dsta<2) return;
  *dst++=1+tm->tm_mon; if (dsta<3) return;
  *dst++=tm->tm_mday; if (dsta<4) return;
  *dst++=tm->tm_hour; if (dsta<5) return;
  *dst++=tm->tm_min; if (dsta<6) return;
  *dst++=tm->tm_sec; if (dsta<7) return;
  struct timeval tv={0};
  gettimeofday(&tv,0);
  *dst=tv.tv_usec/1000;
}

int pbl_get_global_language() {
  return pblrt.lang;
}

void pbl_set_global_language(int lang) {
  if (lang&~0x3ff) return;
  char hi='a'+((lang>>5)&31);
  char lo='a'+(lang&31);
  if ((hi>'z')||(lo>'z')) return;
  if (lang==pblrt.lang) return;
  pblrt.lang=lang;
  pblrt_lang_changed();
}

int pbl_rom_get(void *dst,int dsta) {
  if (pblrt.romc<=dsta) {
    memcpy(dst,pblrt.rom,pblrt.romc);
  }
  return pblrt.romc;
}

int pbl_begin_input_config(int playerid) {
  fprintf(stderr,"%s:%d: TODO %s\n",__FILE__,__LINE__,__func__);
  return -1;
}

/* Store.
 ******************************************************/

int pbl_store_get(char *v,int va,const char *k,int kc) {
  if (!v||(va<0)) va=0;
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  struct sr_decoder decoder={.v=pblrt.store.v,.c=pblrt.store.c};
  if (sr_decode_json_object_start(&decoder)>=0) {
    const char *qk;
    int qkc;
    while ((qkc=sr_decode_json_next(&qk,&decoder))>0) {
      if ((qkc==kc)&&!memcmp(qk,k,kc)) {
        return sr_decode_json_string(v,va,&decoder);
      }
      if (sr_decode_json_skip(&decoder)) break;
    }
  }
  if (va>0) v[0]=0;
  return 0;
}

static int pblrt_replace_store_field(struct sr_encoder *dst,const void *src,int srcc,const char *k,int kc,const char *v,int vc) {
  int got=0;
  if (sr_encode_json_object_start(dst,0,0)<0) return -1;
  struct sr_decoder decoder={.v=src,.c=srcc};
  if (sr_decode_json_object_start(&decoder)>=0) {
    const char *qk;
    int qkc;
    while ((qkc=sr_decode_json_next(&qk,&decoder))>0) {
      const char *qv;
      int qvc=sr_decode_json_expression(&qv,&decoder);
      if (qvc<0) return -1;
      if ((qkc==kc)&&!memcmp(qk,k,kc)) {
        if (sr_encode_json_string(dst,k,kc,v,vc)<0) return -1;
        got=1;
      } else {
        if (sr_encode_json_preencoded(dst,k,kc,qv,qvc)<0) return -1;
      }
    }
  }
  if (!got) {
    if (sr_encode_json_string(dst,k,kc,v,vc)<0) return -1;
  }
  if (sr_encode_json_end(dst,0)<0) return -1;
  return 0;
}

int pbl_store_set(const char *k,int kc,const char *v,int vc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  struct sr_encoder replaced={0};
  if (pblrt_replace_store_field(&replaced,pblrt.store.v,pblrt.store.c,k,kc,v,vc)<0) {
    sr_encoder_cleanup(&replaced);
    return -1;
  }
  sr_encoder_cleanup(&pblrt.store);
  pblrt.store=replaced;
  if (pblrt.storepath) {
    if (file_write(pblrt.storepath,pblrt.store.v,pblrt.store.c)<0) {
      fprintf(stderr,"%s: Failed to write saved game, %d bytes.\n",pblrt.storepath,pblrt.store.c);
    } else {
      fprintf(stderr,"%s: Saved game, %d bytes.\n",pblrt.storepath,pblrt.store.c);
    }
  } else {
    fprintf(stderr,
      "%s:WARNING: Game tried to save '%.*s'='%.*s' but we don't have a save file.\n",
      pblrt.romname,kc,k,vc,v
    );
  }
  return 0;
}

int pbl_store_key_by_index(char *k,int ka,int p) {
  if (!k||(ka<0)) ka=0;
  if (p<0) return -1;
  struct sr_decoder decoder={.v=pblrt.store.v,.c=pblrt.store.c};
  if (sr_decode_json_object_start(&decoder)>=0) {
    const char *qk;
    int qkc;
    while ((qkc=sr_decode_json_next(&qk,&decoder))>0) {
      if (!p--) {
        if (qkc<=ka) {
          memcpy(k,qk,qkc);
          if (qkc<ka) k[qkc]=0;
        }
        return qkc;
      }
      if (sr_decode_json_skip(&decoder)) break;
    }
  }
  return -1;
}

void pblrt_load_store() {
  if (!pblrt.storepath) return;
  void *src=0;
  int srcc=file_read(&src,pblrt.storepath);
  if (srcc>=0) {
    fprintf(stderr,"%s: Loaded saved game, %d bytes.\n",pblrt.storepath,srcc);
    if (pblrt.store.v) free(pblrt.store.v);
    pblrt.store.v=src;
    pblrt.store.c=srcc;
    pblrt.store.a=srcc;
  } else {
    fprintf(stderr,"%s: Failed to load saved game.\n",pblrt.storepath);
    pblrt.store.c=0;
  }
}
