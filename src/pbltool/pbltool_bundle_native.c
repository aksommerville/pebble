#include "pbltool_internal.h"
#include <stdarg.h>
#include <unistd.h>

/* Helper, wrapper around system().
 */
 
static int pbltool_run_shell(const char *fmt,...) {
  va_list vargs;
  va_start(vargs,fmt);
  char cmd[4096];
  int cmdc=vsnprintf(cmd,sizeof(cmd),fmt,vargs);
  if ((cmdc<1)||(cmdc>=sizeof(cmd))) return -1;
  int err=system(cmd);
  if (err) {
    fprintf(stderr,"%s: Shell command failed:\n  %s\n",pbltool.exename,cmd);
    return -1;
  }
  return 0;
}

/* Context for holding paths and such.
 */
 
struct pbltool_bundle_context {
  char *modrompath; // If we're rewriting the ROM. (ie true-native)
  char *asmpath; // Assembly bootstrap to incbin it.
  char *objpath; // Output of that assembly.
  char *cpath; // Text from wasm2c.
  char *cobjpath; // Compiled object from wasm2c.
};

static void pbltool_bundle_context_cleanup(struct pbltool_bundle_context *ctx) {
  if (ctx->modrompath) {
    unlink(ctx->modrompath);
    free(ctx->modrompath);
  }
  if (ctx->asmpath) {
    unlink(ctx->asmpath);
    free(ctx->asmpath);
  }
  if (ctx->objpath) {
    unlink(ctx->objpath);
    // CC usually has '-MMD', which produces an undesired '.d' file too.
    // Rather than fixing that problem, just look for the '.d' file and delete it.
    int c=0; while (ctx->objpath[c]) c++;
    if ((c>=2)&&!memcmp(ctx->objpath+c-2,".o",2)) {
      ctx->objpath[c-1]='d';
      unlink(ctx->objpath);
    }
    free(ctx->objpath);
  }
  if (ctx->cpath) {
    //unlink(ctx->cpath);
    free(ctx->cpath);
  }
  if (ctx->cobjpath) {
    unlink(ctx->cobjpath);
    int c=0; while (ctx->objpath[c]) c++;
    if ((c>=2)&&!memcmp(ctx->objpath+c-2,".o",2)) {
      ctx->objpath[c-1]='d';
      unlink(ctx->objpath);
    }
    free(ctx->cobjpath);
  }
}

/* Generate the 2 or 3 temporary paths.
 */
 
static char *pbltool_path_append(const char *a,const char *b) {
  int ac=0; while (a[ac]) ac++;
  int bc=0; while (b[bc]) bc++;
  int cc=ac+bc;
  char *c=malloc(cc+1);
  if (!c) return 0;
  memcpy(c,a,ac);
  memcpy(c+ac,b,bc);
  c[cc]=0;
  return c;
}
 
static int pbltool_bundle_prepare_paths(struct pbltool_bundle_context *ctx,const char *rompath,int need_mod,int need_wasm2c) {
  if (need_mod) {
    if (!(ctx->modrompath=pbltool_path_append(rompath,".mod"))) return -1;
  }
  if (!(ctx->asmpath=pbltool_path_append(rompath,".s"))) return -1;
  if (!(ctx->objpath=pbltool_path_append(rompath,".o"))) return -1;
  if (need_wasm2c) {
    if (!(ctx->cpath=pbltool_path_append(rompath,".w2c.c"))) return -1;
    if (!(ctx->cobjpath=pbltool_path_append(rompath,".w2c.o"))) return -1;
  }
  return 0;
}

/* Write the assembly file.
 */
 
static int pbltool_bundle_generate_asm(struct pbltool_bundle_context *ctx,const char *rompath) {
  if (ctx->modrompath) rompath=ctx->modrompath;
  struct sr_encoder encoder={0};
  if (sr_encode_fmt(&encoder,
    ".globl pbl_embedded_rom,pbl_embedded_rom_size\n"
    "pbl_embedded_rom:\n"
    ".incbin \"%s\"\n"
    "pbl_embedded_rom_size:\n"
    ".int (pbl_embedded_rom_size-pbl_embedded_rom)\n"
  ,rompath)<0) {
    sr_encoder_cleanup(&encoder);
    return -1;
  }
  if (file_write(ctx->asmpath,encoder.v,encoder.c)<0) {
    fprintf(stderr,"%s: Failed to write intermediate assembly file, %d bytes\n",ctx->asmpath,encoder.c);
    sr_encoder_cleanup(&encoder);
    return -2;
  }
  sr_encoder_cleanup(&encoder);
  return 0;
}

/* Assemble the linkable ROM object.
 * It's just an incbin and some exports, the actual work here is trivial.
 * But we're pretty far abstracted out, lots of configuration type stuff can fail around here.
 */
 
static int pbltool_bundle_assemble(struct pbltool_bundle_context *ctx) {
  return pbltool_run_shell(
    "%s -xassembler-with-cpp -o%s %s",
    PBL_CC,ctx->objpath,ctx->asmpath
  );
}

/* Drop code:1, reencode the ROM, and stash it in (modrompath).
 * We're allowed to damage (rom) along the way.
 */
 
static int pbltool_bundle_rewrite_rom_without_code(struct pbltool_bundle_context *ctx,struct pbltool_rom *rom) {
  if (!ctx->modrompath) return -1;
  // It's supposed to be the second entry. But search generically anyway.
  struct pbltool_res *res=rom->resv;
  int i=rom->resc;
  for (;i-->0;res++) {
    if (res->tid>PBL_TID_code) break;
    if (res->tid<PBL_TID_code) continue;
    res->serialc=0;
  }
  int err=pbltool_rom_validate(rom);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Rewritten ROM failed validation.\n",ctx->modrompath);
    return -2;
  }
  struct sr_encoder dst={0};
  if ((err=pbltool_rom_encode(&dst,rom))<0) {
    if (err!=-2) fprintf(stderr,"%s: Failed to reencode ROM.\n",ctx->modrompath);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  if (file_write(ctx->modrompath,dst.v,dst.c)<0) {
    fprintf(stderr,"%s: Failed to write file, %d bytes\n",ctx->modrompath,dst.c);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  sr_encoder_cleanup(&dst);
  return 0;
}

/* bundle true-native with a library of the game's code.
 */
 
int pbltool_bundle_true_lib(const char *dstpath,struct pbltool_rom *rom,const char *rompath,const char *libpath) {
  struct pbltool_bundle_context ctx={0};
  int err;
  
  // Drop code:1 from rom and incbin it to a linkable object file.
  if (
    ((err=pbltool_bundle_prepare_paths(&ctx,rompath,1,0))<0)||
    ((err=pbltool_bundle_rewrite_rom_without_code(&ctx,rom))<0)||
    ((err=pbltool_bundle_generate_asm(&ctx,rompath))<0)||
    ((err=pbltool_bundle_assemble(&ctx))<0)
  ) {
    pbltool_bundle_context_cleanup(&ctx);
    return err;
  }
  
  // Link.
  if ((err=pbltool_run_shell(
    "%s -o%s -Wl,--start-group %s/out/%s/libpebblenative.a %s %s -Wl,--end-group %s",
    PBL_LD,dstpath,PBL_SDK,PBL_NATIVE_TARGET,ctx.objpath,libpath,PBL_LDPOST
  ))<0) {
    pbltool_bundle_context_cleanup(&ctx);
    return err;
  }
  
  pbltool_bundle_context_cleanup(&ctx);
  return 0;
}

/* Generate C text from the code resource, and unmangle some symbols.
 */
 
static int pbltool_bundle_wasm2c(struct pbltool_bundle_context *ctx,const char *wabt_sdk,struct pbltool_rom *rom,const char *rompath) {
  const uint8_t *src=0;
  int srcc=0;
  const struct pbltool_res *res=rom->resv;
  int i=rom->resc;
  for (;i-->0;res++) {
    if (res->tid!=PBL_TID_code) continue;
    if (res->rid!=1) continue;
    src=res->serial;
    srcc=res->serialc;
    break;
  }
  if (srcc<1) {
    fprintf(stderr,"%s: code:1 not found!\n",rompath);
    return -2;
  }
  
  char cmd[1024];
  int cmdc=snprintf(cmd,sizeof(cmd),"%s/bin/wasm2c - -o %s --module-name=mm",WABT_SDK,ctx->cpath);
  if ((cmdc<1)||(cmdc>=sizeof(cmd))) return -1;
  FILE *pipe=popen(cmd,"w");
  if (!pipe) {
    fprintf(stderr,"%s: Failed to open pipe for Wasm decompilation:\n%s\n",pbltool.exename,cmd);
    return -2;
  }
  
  int srcp=0;
  while (srcp<srcc) {
    int err=fwrite(src+srcp,1,srcc-srcp,pipe);
    if (err<=0) {
      fprintf(stderr,"%s: Writing to Wasm decompilation pipe failed:\n%s\n",pbltool.exename,cmd);
      pclose(pipe);
      return -2;
    }
    srcp+=err;
  }
  pclose(pipe);
  
  return pbltool_run_shell(
    "%s -o%s %s -I%s/wasm2c -I%s/third_party/wasm-c-api/include",
    PBL_CC,ctx->cobjpath,ctx->cpath,WABT_SDK,WABT_SDK
  );
}

/* bundle true-native by recompiling code:1 to the host architecture.
 * This is a very interesting case, I'm not sure how possible it is.
 */
 
int pbltool_bundle_true_recompile(const char *dstpath,struct pbltool_rom *rom,const char *rompath) {
  
  const char *wabt_sdk=0;
  #ifdef WABT_SDK
    wabt_sdk=WABT_SDK;
  #endif
  if (!wabt_sdk||!wabt_sdk[0]) {
    fprintf(stderr,"%s: Please recompile pbltool with WABT_SDK defined, to use `pbltool --recompile`.\n",pbltool.exename);
    return -2;
  }
  
  struct pbltool_bundle_context ctx={0};
  int err;
  
  // Sundry prep work, and generate the assembled rom object file and the C text from code:1.
  if (
    ((err=pbltool_bundle_prepare_paths(&ctx,rompath,1,1))<0)||
    ((err=pbltool_bundle_wasm2c(&ctx,wabt_sdk,rom,rompath))<0)|| // Must do before rewriting ROM.
    ((err=pbltool_bundle_rewrite_rom_without_code(&ctx,rom))<0)||
    ((err=pbltool_bundle_generate_asm(&ctx,rompath))<0)||
    ((err=pbltool_bundle_assemble(&ctx))<0)
  ) {
    pbltool_bundle_context_cleanup(&ctx);
    return err;
  }
  
  // Compile and link it all.
  if ((err=pbltool_run_shell(
    "%s -o%s %s %s %s/out/%s/libpebblewasm2c.a %s",
    PBL_LD,dstpath,ctx.objpath,ctx.cobjpath,PBL_SDK,PBL_NATIVE_TARGET,PBL_LDPOST
  ))<0) {
    pbltool_bundle_context_cleanup(&ctx);
    return err;
  }
  
  pbltool_bundle_context_cleanup(&ctx);
  return 0;
}

/* bundle fake-native.
 */
 
int pbltool_bundle_fake(const char *dstpath,const char *rompath) {
  struct pbltool_bundle_context ctx={0};
  int err;
  
  // Shovel (rompath) into a linkable object file.
  // Alas with `objcopy` we can't control the object name to my satisfaction, so we use assembler instead.
  if (
    ((err=pbltool_bundle_prepare_paths(&ctx,rompath,0,0))<0)||
    ((err=pbltool_bundle_generate_asm(&ctx,rompath))<0)||
    ((err=pbltool_bundle_assemble(&ctx))<0)
  ) {
    pbltool_bundle_context_cleanup(&ctx);
    return err;
  }
  
  // Link.
  if ((err=pbltool_run_shell(
    "%s -o%s %s %s/out/%s/libpebblefake.a %s",
    PBL_LD,dstpath,ctx.objpath,PBL_SDK,PBL_NATIVE_TARGET,PBL_LDPOST
  ))<0) {
    pbltool_bundle_context_cleanup(&ctx);
    return err;
  }
  
  pbltool_bundle_context_cleanup(&ctx);
  return 0;
}
