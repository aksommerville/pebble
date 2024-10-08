/* pblrt_exec.c
 * Defines our execution model.
 * This should be the only file that knows whether our game is WebAssembly or Native.
 * It gets compiled twice, once for each option.
 */

#include "pblrt_internal.h"

#define WASM 1
#define NATIVE 2
#define WASM2C 3

#if EXECFMT==WASM
#include "wasm_c_api.h"
#include "wasm_export.h"

static struct pblrt_exec {
  wasm_module_t mod;
  wasm_module_inst_t inst;
  wasm_exec_env_t ee;
  
  // Wamr evidently writes into the binary we give it. Why would it do that? Well, we have to copy it then.
  void *code;
  int codec;
  
  wasm_function_inst_t pbl_client_quit;
  wasm_function_inst_t pbl_client_init;
  wasm_function_inst_t pbl_client_update;
  wasm_function_inst_t pbl_client_render;
  wasm_function_inst_t pbl_client_synth;
} pblrt_exec={0};

static void *pblrt_exec_get_client_memory(uint32_t addr,int len) {
  if (!wasm_runtime_validate_app_addr(pblrt_exec.inst,addr,len)) return 0;
  return wasm_runtime_addr_app_to_native(pblrt_exec.inst,addr);
}

/* pbl_log
 */
 
static void pbl_wasm_log(wasm_exec_env_t ee,const char *fmt,int vargs) {
  char msg[256];
  int msgc=0,err;
  while (*fmt) {
    if (*fmt!='%') {
      if (msgc<sizeof(msg)) msg[msgc++]=*fmt;
      fmt++;
      continue;
    }
    fmt++;
    #define RD32(name) \
      int name=0; \
      { \
        const int *vp=pblrt_exec_get_client_memory(vargs,4); \
        if (vp) name=*vp; \
      } \
      vargs+=4;
    switch (fmt[0]) {
      case '%': if (msgc<sizeof(msg)) msg[msgc++]='%'; fmt++; break;
      case 'd': {
          fmt++;
          RD32(v)
          err=sr_decsint_repr(msg+msgc,sizeof(msg)-msgc,v);
          if (msgc<=sizeof(msg)-err) msgc+=err;
        } break;
      case 'x': {
          fmt++;
          RD32(v)
          err=sr_hexuint_repr(msg+msgc,sizeof(msg)-msgc,v,0,0);
          if (msgc<=sizeof(msg)-err) msgc+=err;
        } break;
      case 's': {
          fmt++;
          RD32(p)
          if (!p) break;
          if (!wasm_runtime_validate_app_str_addr(pblrt_exec.inst,p)) break;
          const char *src=wasm_runtime_addr_app_to_native(pblrt_exec.inst,p);
          if (!src) break;
          int srcc=0; while ((srcc<256)&&src[srcc]) srcc++;
          if (msgc<=sizeof(msg)-srcc) {
            memcpy(msg+msgc,src,srcc);
            msgc+=srcc;
          }
        } break;
      case 'f': {
          fmt++;
          if (vargs&4) vargs+=4;
          double v=0.0;
          const double *vp=pblrt_exec_get_client_memory(vargs,8);
          if (vp) v=*vp;
          vargs+=8;
          err=sr_double_repr(msg+msgc,sizeof(msg)-msgc,v);
          if (msgc<=sizeof(msg)-err) msgc+=err;
        } break;
      case 'p': {
          // 'p' for pointer, but not host pointers -- it's always 32 bits in wasm.
          fmt++;
          RD32(v)
          if (msgc<=sizeof(msg)-10) {
            msg[msgc++]='0';
            msg[msgc++]='x';
            msg[msgc++]="0123456789abcdef"[(v>>28)&15];
            msg[msgc++]="0123456789abcdef"[(v>>24)&15];
            msg[msgc++]="0123456789abcdef"[(v>>20)&15];
            msg[msgc++]="0123456789abcdef"[(v>>16)&15];
            msg[msgc++]="0123456789abcdef"[(v>>12)&15];
            msg[msgc++]="0123456789abcdef"[(v>> 8)&15];
            msg[msgc++]="0123456789abcdef"[(v>> 4)&15];
            msg[msgc++]="0123456789abcdef"[(v    )&15];
          }
        } break;
      case '.': {
          if ((fmt[1]=='*')&&(fmt[2]=='s')) {
            fmt+=3;
            RD32(len)
            if (len<0) len=0;
            RD32(p)
            if (p&&len) {
              const char *src=pblrt_exec_get_client_memory(p,len);
              if (src) {
                if (msgc<=sizeof(msg)-len) {
                  memcpy(msg+msgc,src,len);
                  msgc+=len;
                }
              }
            }
          } else {
            if (msgc<sizeof(msg)) msg[msgc++]='%';
          }
        } break;
      default: {
          if (msgc<sizeof(msg)) msg[msgc++]='%';
        }
    }
  }
  fprintf(stderr,"GAME: %.*s\n",msgc,msg);
}

/* Minor client entry points.
 * Really trivial things, we'll implement inline.
 * Where there's real behavior happening, just bridge to the native API entry points.
 */

static void pbl_wasm_terminate(wasm_exec_env_t ee,int status) {
  pblrt.terminate++;
  pblrt.termstatus=(status<0)?1:status;
}

static void pbl_wasm_set_synth_limit(wasm_exec_env_t ee,int samplec) {
  pblrt.synth_limit=samplec;
}

static double pbl_wasm_now_real(wasm_exec_env_t ee) {
  return pbl_now_real();
}

static void pbl_wasm_now_local(wasm_exec_env_t ee,int dstp,int dsta) {
  if (dsta<1) return;
  if (dsta>7) dsta=7;
  int *dst=pblrt_exec_get_client_memory(dstp,4*dsta);
  if (!dst) return;
  pbl_now_local(dst,dsta);
}

static int pbl_wasm_get_global_language(wasm_exec_env_t ee) {
  return pblrt.lang;
}

static void pbl_wasm_set_global_language(wasm_exec_env_t ee,int lang) {
  pbl_set_global_language(lang);
}

static int pbl_wasm_begin_input_config(wasm_exec_env_t ee,int playerid) {
  return pbl_begin_input_config(playerid);
}

static int pbl_wasm_store_get(wasm_exec_env_t ee,char *v,int va,const char *k,int kc) {
  return pbl_store_get(v,va,k,kc);
}

static int pbl_wasm_store_set(wasm_exec_env_t ee,const char *k,int kc,const char *v,int vc) {
  return pbl_store_set(k,kc,v,vc);
}

static int pbl_wasm_store_key_by_index(wasm_exec_env_t ee,char *k,int ka,int p) {
  return pbl_store_key_by_index(k,ka,p);
}

static int pbl_wasm_rom_get(wasm_exec_env_t ee,void *dst,int dsta) {
  if (pblrt.romc<=dsta) {
    memcpy(dst,pblrt.rom,pblrt.romc);
  }
  return pblrt.romc;
}

static NativeSymbol pblrt_exec_exports[]={
  {"pbl_log",pbl_wasm_log,"($i)"},
  {"pbl_terminate",pbl_wasm_terminate,"(i)"},
  {"pbl_set_synth_limit",pbl_wasm_set_synth_limit,"(i)"},
  {"pbl_now_real",pbl_wasm_now_real,"()F"},
  {"pbl_now_local",pbl_wasm_now_local,"(ii)"},
  {"pbl_get_global_language",pbl_wasm_get_global_language,"()i"},
  {"pbl_set_global_language",pbl_wasm_set_global_language,"(i)"},
  {"pbl_begin_input_config",pbl_wasm_begin_input_config,"(i)i"},
  {"pbl_store_get",pbl_wasm_store_get,"(*~*~)i"},
  {"pbl_store_set",pbl_wasm_store_set,"(*~*~)i"},
  {"pbl_store_key_by_index",pbl_wasm_store_key_by_index,"(*~i)i"},
  {"pbl_rom_get",pbl_wasm_rom_get,"(*~)i"},
};

#elif EXECFMT==NATIVE

// The Pebble API with native linkage is included in all cases. (pblrt_exec_native_api.c)
// A lot of it is redundant in the WASM case, but not all.
// It's tiny anyway, no worries to include.

#elif EXECFMT==WASM2C

#include "wasm-rt.h"

struct w2c_env;

typedef struct w2c_mm {
  struct w2c_env* w2c_env_instance;
  uint32_t w2c_0x5F_stack_pointer;
  wasm_rt_memory_t w2c_memory;
  wasm_rt_funcref_table_t w2c_0x5F_indirect_function_table;
} w2c_mm;

static struct pblrt_exec {
  w2c_mm mod;
} pblrt_exec={0};

#define HOSTADDR(wasmaddr,len) (void*)(((wasmaddr)<=pblrt_exec.mod.w2c_memory.size-(len))?(pblrt_exec.mod.w2c_memory.data+(wasmaddr)):0)

void wasm2c_mm_instantiate(w2c_mm* instance);
void wasm2c_mm_free(w2c_mm* instance);
void w2c_mm_pbl_client_quit(w2c_mm *mod,int termstatus);
int w2c_mm_pbl_client_init(w2c_mm *mod,int fbw,int fbh,int rate,int chanc);
void w2c_mm_pbl_client_update(w2c_mm *mod,double elapsed,int in1,int in2,int in3,int in4);
uint32_t w2c_mm_pbl_client_render(w2c_mm *mod);
uint32_t w2c_mm_pbl_client_synth(w2c_mm *mod,int c);

void w2c_env_pbl_log(struct w2c_env *bother,uint32_t fmtp,uint32_t vargs) {
  const char *fmt=HOSTADDR(fmtp,0);
  if (!fmt) return;
  char msg[256];
  int msgc=0,err;
  while (*fmt) {
    if (*fmt!='%') {
      if (msgc<sizeof(msg)) msg[msgc++]=*fmt;
      fmt++;
      continue;
    }
    fmt++;
    #define RD32(name) \
      int name=0; \
      { \
        const int *vp=HOSTADDR(vargs,4); \
        if (vp) name=*vp; \
      } \
      vargs+=4;
    switch (fmt[0]) {
      case '%': if (msgc<sizeof(msg)) msg[msgc++]='%'; fmt++; break;
      case 'd': {
          fmt++;
          RD32(v)
          err=sr_decsint_repr(msg+msgc,sizeof(msg)-msgc,v);
          if (msgc<=sizeof(msg)-err) msgc+=err;
        } break;
      case 'x': {
          fmt++;
          RD32(v)
          err=sr_hexuint_repr(msg+msgc,sizeof(msg)-msgc,v,0,0);
          if (msgc<=sizeof(msg)-err) msgc+=err;
        } break;
      case 's': {
          fmt++;
          RD32(p)
          if (!p) break;
          const char *src=HOSTADDR(p,0);
          if (!src) break;
          int srcc=0; while ((srcc<256)&&src[srcc]) srcc++;
          if (msgc<=sizeof(msg)-srcc) {
            memcpy(msg+msgc,src,srcc);
            msgc+=srcc;
          }
        } break;
      case 'f': {
          fmt++;
          if (vargs&4) vargs+=4;
          double v=0.0;
          const double *vp=HOSTADDR(vargs,8);
          if (vp) v=*vp;
          vargs+=8;
          err=sr_double_repr(msg+msgc,sizeof(msg)-msgc,v);
          if (msgc<=sizeof(msg)-err) msgc+=err;
        } break;
      case 'p': {
          // 'p' for pointer, but not host pointers -- it's always 32 bits in wasm.
          fmt++;
          RD32(v)
          if (msgc<=sizeof(msg)-10) {
            msg[msgc++]='0';
            msg[msgc++]='x';
            msg[msgc++]="0123456789abcdef"[(v>>28)&15];
            msg[msgc++]="0123456789abcdef"[(v>>24)&15];
            msg[msgc++]="0123456789abcdef"[(v>>20)&15];
            msg[msgc++]="0123456789abcdef"[(v>>16)&15];
            msg[msgc++]="0123456789abcdef"[(v>>12)&15];
            msg[msgc++]="0123456789abcdef"[(v>> 8)&15];
            msg[msgc++]="0123456789abcdef"[(v>> 4)&15];
            msg[msgc++]="0123456789abcdef"[(v    )&15];
          }
        } break;
      case '.': {
          if ((fmt[1]=='*')&&(fmt[2]=='s')) {
            fmt+=3;
            RD32(len)
            if (len<0) len=0;
            RD32(p)
            if (p&&len) {
              const char *src=HOSTADDR(p,len);
              if (src) {
                if (msgc<=sizeof(msg)-len) {
                  memcpy(msg+msgc,src,len);
                  msgc+=len;
                }
              }
            }
          } else {
            if (msgc<sizeof(msg)) msg[msgc++]='%';
          }
        } break;
      default: {
          if (msgc<sizeof(msg)) msg[msgc++]='%';
        }
    }
  }
  fprintf(stderr,"GAME: %.*s\n",msgc,msg);
}

void w2c_env_pbl_terminate(struct w2c_env *bother,int status) {
  pbl_terminate(status);
}

void w2c_env_pbl_set_synth_limit(struct w2c_env *bother,int samplec) {
  pbl_set_synth_limit(samplec);
}

double w2c_env_pbl_now_real(struct w2c_env *bother) {
  return pbl_now_real();
}

void w2c_env_pbl_now_local(struct w2c_env *bother,uint32_t dstp,int dsta) {
  pbl_now_local(HOSTADDR(dstp,dsta*sizeof(int)),dsta);
}

int w2c_env_pbl_get_global_language(struct w2c_env *bother) {
  return pbl_get_global_language();
}

void w2c_env_pbl_set_global_language(struct w2c_env *bother,int lang) {
  pbl_set_global_language(lang);
}

int w2c_env_pbl_begin_input_config(struct w2c_env *bother,int playerid) {
  return pbl_begin_input_config(playerid);
}

int w2c_env_pbl_store_get(struct w2c_env *bother,uint32_t vp,int va,uint32_t kp,int kc) {
  return pbl_store_get(HOSTADDR(vp,va),va,HOSTADDR(kp,kc),kc);
}

int w2c_env_pbl_store_set(struct w2c_env *bother,uint32_t kp,int kc,uint32_t vp,int vc) {
  return pbl_store_set(HOSTADDR(kp,kc),kc,HOSTADDR(vp,vc),vc);
}

int w2c_env_pbl_store_key_by_index(struct w2c_env *bother,uint32_t kp,int ka,int p) {
  return pbl_store_key_by_index(HOSTADDR(kp,ka),ka,p);
}

int w2c_env_pbl_rom_get(struct w2c_env *bother,uint32_t dstp,int dsta) {
  return pbl_rom_get(HOSTADDR(dstp,dsta),dsta);
}

#else
  #error "Please compile with either -DEXECFMT=WASM or -DEXECFMT=NATIVE."
#endif

/* Quit.
 */
 
void pblrt_exec_quit() {
  #if EXECFMT==WASM
    if (pblrt_exec.ee) wasm_runtime_destroy_exec_env(pblrt_exec.ee);
    if (pblrt_exec.inst) wasm_runtime_deinstantiate(pblrt_exec.inst);
    if (pblrt_exec.mod) wasm_module_delete(&pblrt_exec.mod);
  #elif EXECFMT==WASM2C
    wasm2c_mm_free(&pblrt_exec.mod);
    wasm_rt_free();
  #endif
}

/* Init.
 */
 
int pblrt_exec_init() {
  #if EXECFMT==WASM
    {
      void *src=0;
      int srcc=pblrt_rom_get(&src,PBL_TID_code,1);
      if (srcc<1) {
        fprintf(stderr,"%s: code:1 not found\n",pblrt.romname);
        return -2;
      }
      if (!(pblrt_exec.code=malloc(srcc))) return -1;
      memcpy(pblrt_exec.code,src,srcc);
      pblrt_exec.codec=srcc;
    }
  
    if (!wasm_runtime_init()) return -1;
    if (!wasm_runtime_register_natives("env",pblrt_exec_exports,sizeof(pblrt_exec_exports)/sizeof(NativeSymbol))) return -1;
  
    int stack_size=0x01000000;//TODO
    int heap_size=0x01000000;//TODO
    char msg[1024]={0};
    if (!(pblrt_exec.mod=wasm_runtime_load(pblrt_exec.code,pblrt_exec.codec,msg,sizeof(msg)))) {
      fprintf(stderr,"%s: wasm_runtime_load failed: %s\n",pblrt.romname,msg);
      return -2;
    }
    if (!(pblrt_exec.inst=wasm_runtime_instantiate(pblrt_exec.mod,stack_size,heap_size,msg,sizeof(msg)))) {
      fprintf(stderr,"%s: wasm_runtime_instantiate failed: %s\n",pblrt.romname,msg);
      return -2;
    }
    if (!(pblrt_exec.ee=wasm_runtime_create_exec_env(pblrt_exec.inst,stack_size))) {
      fprintf(stderr,"%s: wasm_runtime_create_exec_env failed\n",pblrt.romname);
      return -2;
    }
  
    #define LOADFN(name) { \
      if (!(pblrt_exec.name=wasm_runtime_lookup_function(pblrt_exec.inst,#name))) { \
        fprintf(stderr,"%s: ROM does not export required function '%s'\n",pblrt.romname,#name); \
        return -2; \
      } \
    }
    LOADFN(pbl_client_quit)
    LOADFN(pbl_client_init)
    LOADFN(pbl_client_update)
    LOADFN(pbl_client_render)
    LOADFN(pbl_client_synth)
    #undef LOADFN
  
  #elif EXECFMT==NATIVE
  
  #elif EXECFMT==WASM2C
    wasm_rt_init();
    wasm2c_mm_instantiate(&pblrt_exec.mod);
  #endif
  return 0;
}

/* Update.
 */
 
int pblrt_exec_update(double elapsed) {
  #if EXECFMT==WASM
    uint32_t argv[6]={0,0,pblrt.instate[0],pblrt.instate[1],pblrt.instate[2],pblrt.instate[3]};
    memcpy(argv,&elapsed,sizeof(double));
    if (wasm_runtime_call_wasm(pblrt_exec.ee,pblrt_exec.pbl_client_update,6,argv)) return 0;
    const char *msg=wasm_runtime_get_exception(pblrt_exec.inst);
    fprintf(stderr,"%s: pbl_client_update failed hard: %s\n",pblrt.romname,msg);
    return -2;
  
  #elif EXECFMT==NATIVE
    pbl_client_update(elapsed,pblrt.instate[0],pblrt.instate[1],pblrt.instate[2],pblrt.instate[3]);
    
  #elif EXECFMT==WASM2C
    w2c_mm_pbl_client_update(&pblrt_exec.mod,elapsed,pblrt.instate[0],pblrt.instate[1],pblrt.instate[2],pblrt.instate[3]);
    
  #endif
  return 0;
}

/* Call pbl_client_init().
 */
 
int pblrt_exec_call_init() {
  #if EXECFMT==WASM
    uint32_t argv[4]={
      pblrt.fbw,pblrt.fbh,
    };
    if (pblrt.audio) {
      argv[2]=pblrt.audio->rate;
      argv[3]=pblrt.audio->chanc;
    }
    if (wasm_runtime_call_wasm(pblrt_exec.ee,pblrt_exec.pbl_client_init,4,argv)) {
      int result=argv[0];
      if (result<0) {
        fprintf(stderr,"%s: Error %d from pbl_client_init\n",pblrt.romname,result);
        return -2;
      }
    } else {
      const char *msg=wasm_runtime_get_exception(pblrt_exec.inst);
      fprintf(stderr,"%s: pbl_client_init failed hard: %s\n",pblrt.romname,msg);
      return -2;
    }
    return 0;
  
  #elif EXECFMT==NATIVE
    int rate=0,chanc=0;
    if (pblrt.audio) {
      rate=pblrt.audio->rate;
      chanc=pblrt.audio->chanc;
    }
    return pbl_client_init(pblrt.fbw,pblrt.fbh,rate,chanc);
    
  #elif EXECFMT==WASM2C
    int rate=0,chanc=0;
    if (pblrt.audio) {
      rate=pblrt.audio->rate;
      chanc=pblrt.audio->chanc;
    }
    return w2c_mm_pbl_client_init(&pblrt_exec.mod,pblrt.fbw,pblrt.fbh,rate,chanc);
    
  #endif
  return 0;
}

/* Call pbl_client_quit().
 */
 
void pblrt_exec_call_quit() {
  #if EXECFMT==WASM
    uint32_t argv[1]={pblrt.termstatus};
    if (!wasm_runtime_call_wasm(pblrt_exec.ee,pblrt_exec.pbl_client_quit,1,argv)) {
      const char *msg=wasm_runtime_get_exception(pblrt_exec.inst);
      fprintf(stderr,"%s: pbl_client_quit failed hard: %s\n",pblrt.romname,msg);
    }
    
  #elif EXECFMT==NATIVE
    pbl_client_quit(pblrt.termstatus);
    
  #elif EXECFMT==WASM2C
    w2c_mm_pbl_client_quit(&pblrt_exec.mod,pblrt.termstatus);
    
  #endif
}

/* Call pbl_client_render().
 */
 
int pblrt_exec_call_render(void **fbpp) {
  #if EXECFMT==WASM
    uint32_t argv[1]={0};
    if (!wasm_runtime_call_wasm(pblrt_exec.ee,pblrt_exec.pbl_client_render,0,argv)) {
      const char *msg=wasm_runtime_get_exception(pblrt_exec.inst);
      fprintf(stderr,"%s: pbl_client_render failed hard: %s\n",pblrt.romname,msg);
      return -2;
    }
    if (argv[0]) {
      if (!(*fbpp=pblrt_exec_get_client_memory(argv[0],pblrt.fbw*pblrt.fbh*4))) {
        fprintf(stderr,"%s: pbl_client_render returned invalid pointer 0x%08x\n",pblrt.romname,argv[0]);
        return -2;
      }
    }
  #elif EXECFMT==NATIVE
    *fbpp=pbl_client_render();
  #elif EXECFMT==WASM2C
    uint32_t fbp=w2c_mm_pbl_client_render(&pblrt_exec.mod);
    if (fbp) *fbpp=HOSTADDR(fbp,0);
  #endif
  return 0;
}

/* Call pbl_client_synth() until (pblrt.abuf) is full.
 */
 
int pblrt_exec_refill_audio() {
  if (!pblrt.audio) return 0;
  if (pblrt.synth_limit<1) return 0;
  int reqc=pblrt.abufa-pblrt.abufc;
  while (reqc>0) {
    if (pblrt_audio_lock()<0) return -1;
    int updc=pblrt.abufa-(pblrt.abufp+pblrt.abufc)%pblrt.abufa;
    if (updc>reqc) updc=reqc;
    if (updc>pblrt.synth_limit) updc=pblrt.synth_limit;
    int dstp=pblrt.abufp+pblrt.abufc;
    if (dstp>=pblrt.abufa) dstp-=pblrt.abufa;
    const void *src=0;
    
    #if EXECFMT==WASM
      uint32_t argv[1]={updc};
      if (wasm_runtime_call_wasm(pblrt_exec.ee,pblrt_exec.pbl_client_synth,1,argv)) {
        if (argv[0]) {
          if (!(src=pblrt_exec_get_client_memory(argv[0],updc*2))) {
            fprintf(stderr,"%s: pbl_client_synth returned invalid pointer 0x%08x\n",pblrt.romname,argv[0]);
            pblrt_audio_unlock();
            return -2;
          }
        }
      } else {
        const char *msg=wasm_runtime_get_exception(pblrt_exec.inst);
        fprintf(stderr,"%s: pbl_client_synth failed hard: %s\n",pblrt.romname,msg);
        return -2;
      }
  
    #elif EXECFMT==NATIVE
      src=pbl_client_synth(updc);
    #elif EXECFMT==WASM2C
      uint32_t p=w2c_mm_pbl_client_synth(&pblrt_exec.mod,updc);
      if (p) src=HOSTADDR(p,0);
    #endif
    
    // When they return null, the strictly correct thing to do would be append zeroes to the buffer.
    // Instead, I'm just leaving the buffer short.
    // So if the client has only intermittent audio, we kind of skip the extra buffering of zeroes.
    if (!src) {
      pblrt_audio_unlock();
      return 0;
    }
    memcpy(pblrt.abuf+dstp,src,updc<<1);
    pblrt.abufc+=updc;
    reqc-=updc;
    pblrt_audio_unlock();
  }
  return 0;
}
