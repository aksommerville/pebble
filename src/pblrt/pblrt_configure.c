#include "pblrt_internal.h"

/* --help
 */
 
void pblrt_print_usage() {
  if (pblrt_romsrc_uses_external_rom) {
    fprintf(stderr,"\nUsage: %s ROM [OPTIONS]\n\n",pblrt.exename);
  } else {
    fprintf(stderr,"\nUsage: %s [OPTIONS]\n\n",pblrt.exename);
  }
  fprintf(stderr,
    "Usually omitting all options yields sensible defaults:\n"
    "  --help                     Print this message and exit.\n"
    "  --store=PATH               File for saved data. If empty, saving is disabled.\n"
    "  --lang=ISO639              Force language. Overwrites environment variables.\n"
    "  --video-driver=NAME        See below.\n"
    "  --video-device=NAME        Depends on driver.\n"
    "  --fullscreen=0|1           Start in fullscreen.\n"
    "  --video-size=WxH           Initial window size.\n"
    "  --audio-driver=NAME        See below.\n"
    "  --audio-device=NAME        Depends on driver.\n"
    "  --audio-rate=HZ            Preferred output rate.\n"
    "  --audio-chanc=CHANNELS     Usually 1 or 2.\n"
    "  --mono                     Alias for --audio-chanc=1.\n"
    "  --stereo                   Alias for --audio-chanc=2.\n"
    "  --audio-buffer=SAMPLES     Depends on driver.\n"
    "  --input-driver=NAME        See below.\n"
    "\n"
  );
  {
    fprintf(stderr,"Video drivers:\n");
    int p=0; for (;;p++) {
      const struct pblrt_video_type *type=pblrt_video_type_by_index(p);
      if (!type) break;
      fprintf(stderr,"%15s : %s\n",type->name,type->desc);
    }
    fprintf(stderr,"\n");
  }
  {
    fprintf(stderr,"Audio drivers:\n");
    int p=0; for (;;p++) {
      const struct pblrt_audio_type *type=pblrt_audio_type_by_index(p);
      if (!type) break;
      fprintf(stderr,"%15s : %s\n",type->name,type->desc);
    }
    fprintf(stderr,"\n");
  }
  {
    fprintf(stderr,"Input drivers:\n");
    int p=0; for (;;p++) {
      const struct pblrt_input_type *type=pblrt_input_type_by_index(p);
      if (!type) break;
      fprintf(stderr,"%15s : %s\n",type->name,type->desc);
    }
    fprintf(stderr,"\n");
  }
}

/* Set string.
 */
 
static int pblrt_configure_set_string(char **dstpp,const char *src,int srcc) {
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (*dstpp) free(*dstpp);
  *dstpp=nv;
  return 0;
}

/* Receive key=value option.
 */
 
static int pblrt_configure_kv(const char *k,int kc,const char *v,int vc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  
  if (!v) {
    if ((kc>=3)&&!memcmp(k,"no-",3)) {
      k+=3;
      kc-=3;
      v="0";
      vc=1;
    } else {
      v="1";
      vc=1;
    }
  }
  
  /* First pick off the oddballs.
   */
  
  if ((kc==4)&&!memcmp(k,"help",4)) {
    pblrt.terminate=1;
    pblrt_print_usage();
    return 0;
  }
  
  if ((kc==4)&&!memcmp(k,"lang",4)) {
    if ((vc!=2)||(v[0]<'a')||(v[0]>'z')||(v[1]<'a')||(v[2]>'z')) {
      fprintf(stderr,"%s: Expected 2-letter language code, found '%.*s'\n",pblrt.exename,vc,v);
      return -2;
    }
    pblrt.lang=((v[0]-'a')<<5)|(v[1]-'a');
    return 0;
  }
  
  if ((kc==10)&&!memcmp(k,"video-size",10)) {
    int i=0;
    for (;i<vc;i++) if (v[i]=='x') {
      int w,h;
      if (
        (sr_int_eval(&w,v,i)<2)||
        (sr_int_eval(&h,v+i+1,vc-i-1)<2)||
        (w<1)||(h<1)
      ) break;
      pblrt.video_w=w;
      pblrt.video_h=h;
      return 0;
    }
    fprintf(stderr,"%s: Invalid window size '%.*s'\n",pblrt.exename,vc,v);
    return -2;
  }
  
  if ((kc==4)&&!memcmp(k,"mono",4)) {
    pblrt.audio_chanc=1;
    return 0;
  }
  
  if ((kc==6)&&!memcmp(k,"stereo",6)) {
    pblrt.audio_chanc=2;
    return 0;
  }
  
  if ((kc==5)&&!memcmp(k,"store",5)) pblrt.autostore=0; // pass
    
  /* Every other option follows a pretty standard form.
   */
    
  #define STROPT(optname,fldname) if ((kc==sizeof(optname)-1)&&!memcmp(k,optname,kc)) { \
    return pblrt_configure_set_string(&pblrt.fldname,v,vc); \
  }
  #define INTOPT(optname,fldname) if ((kc==sizeof(optname)-1)&&!memcmp(k,optname,kc)) { \
    int vn; \
    if (sr_int_eval(&vn,v,vc)<2) { \
      fprintf(stderr,"%s: Failed to evaluate '%.*s' as integer for option '%.*s'\n",pblrt.exename,vc,v,kc,k); \
      return -2; \
    } \
    pblrt.fldname=vn; \
    return 0; \
  }
  
  STROPT("store",storepath)
  STROPT("video-driver",video_driver)
  STROPT("video-device",video_device)
  INTOPT("fullscreen",fullscreen)
  STROPT("audio-driver",audio_driver)
  STROPT("audio-device",audio_device)
  INTOPT("audio-rate",audio_rate)
  INTOPT("audio-chanc",audio_chanc)
  INTOPT("audio-buffer",audio_buffer)
  STROPT("input-driver",input_driver)
  
  #undef STROPT
  #undef INTOPT
  
  fprintf(stderr,"%s: Unexpected option '%.*s' = '%.*s'\n",pblrt.exename,kc,k,vc,v);
  return -2;
}

/* Receive argv.
 */
 
static int pblrt_configure_argv(int argc,char **argv) {
  int argi=1,err;
  while (argi<argc) { 
    const char *arg=argv[argi++];
    if (!arg||!arg[0]) continue;
    
    if (arg[0]!='-') {
      if (pblrt_romsrc_uses_external_rom&&!pblrt.rompath) {
        pblrt.rompath=arg;
      } else {
        goto _unexpected_;
      }
      continue;
    }
    
    if (!arg[1]) goto _unexpected_;
    
    if (arg[1]!='-') {
      const char *v=0;
      if (arg[2]) v=arg+2;
      else if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) v=argv[argi++];
      if ((err=pblrt_configure_kv(arg+1,1,v,-1))<0) return err;
      continue;
    }
    
    if (!arg[2]) goto _unexpected_;
    
    if (arg[2]!='-') {
      const char *k=arg+2;
      int kc=0;
      while (k[kc]&&(k[kc]!='=')) kc++;
      const char *v=0;
      if (k[kc]=='=') v=k+kc+1;
      else if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) v=argv[argi++];
      if ((err=pblrt_configure_kv(k,kc,v,-1))<0) return err;
      continue;
    }
    
   _unexpected_:;
    fprintf(stderr,"%s: Unexpected argument '%s'\n",pblrt.exename,arg);
    return -2;
  }
  return 0;
}

/* Finish applying configuration.
 */
 
static int pblrt_configure_finish() {

  // If they didn't supply '--store', make up a default.
  if (pblrt.autostore&&!pblrt.storepath) {
    if (pblrt_romsrc_uses_external_rom) {
      if (pblrt.rompath) {
        // We have an external ROM file. Append ".save" to its path.
        int pfxc=0; while (pblrt.rompath[pfxc]) pfxc++;
        int dstc=pfxc+5;
        if (!(pblrt.storepath=malloc(dstc+1))) return -1;
        memcpy(pblrt.storepath,pblrt.rompath,pfxc);
        memcpy(pblrt.storepath+pfxc,".save",6); // sic 6, copy the terminator too
      }
    } else {
      // If exename contains a slash, it is a path. Otherwise don't assume we can save there, and use the working directory instead.
      // Actually, those end up being the same thing. So just append ".save" to exename.
      int pfxc=0; while (pblrt.exename[pfxc]) pfxc++;
      int dstc=pfxc+5;
      if (!(pblrt.storepath=malloc(dstc+1))) return -1;
      memcpy(pblrt.storepath,pblrt.exename,pfxc);
      memcpy(pblrt.storepath+pfxc,".save",6); // sic 6, copy the terminator too
    }
  }

  return 0;
}

/* Configure, main.
 */
 
int pblrt_configure(int argc,char **argv) {
  int err;
  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) pblrt.exename=argv[0];
  else pblrt.exename="pblrt";
  pblrt.autostore=1;
  //TODO Config file.
  if ((err=pblrt_configure_argv(argc,argv))<0) return err;
  return pblrt_configure_finish();
}

/* Fetch user languages from the environment.
 */
 
static int pblrt_fetch_user_languages(int *dst,int dsta) {
  if (dsta<1) return 0;
  
  /* POSIX systems typically have LANG as the single preferred locale, which starts with a language code.
   * There can also be LANGUAGE, which is multiple language codes separated by colons.
   */
  int dstc=0;
  const char *src;
  if (src=getenv("LANG")) {
    if ((src[0]>='a')&&(src[0]<='z')&&(src[1]>='a')&&(src[1]<='z')) {
      if (dstc<dsta) dst[dstc++]=((src[0]-'a')<<5)|(src[1]-'a');
    }
  }
  if (dstc>=dsta) return dstc;
  if (src=getenv("LANGUAGE")) {
    int srcp=0;
    while (src[srcp]&&(dstc<dsta)) {
      const char *token=src+srcp;
      int tokenc=0;
      while (src[srcp]&&(src[srcp++]!=':')) tokenc++;
      if ((tokenc>=2)&&(token[0]>='a')&&(token[0]<='z')&&(token[1]>='a')&&(token[1]<='z')) {
        int lang=((token[0]-'a')<<5)|(token[1]-'a');
        int already=0,i=dstc;
        while (i-->0) if (dst[i]==lang) { already=1; break; }
        if (!already) {
          dst[dstc++]=lang;
          if (dstc>=dsta) return dstc;
        }
      }
    }
  }
  
  //TODO Alternate methods for MacOS and Windows, surely they exist.
  
  return dstc;
}

/* Select default language.
 * (pblrt.lang) would be set if the user provided --lang at command line.
 * We should only be called when that's zero.
 */

int pblrt_choose_default_language() {

  // Find the user's preferred languages.
  int user[16];
  int userc=pblrt_fetch_user_languages(user,sizeof(user)/sizeof(user[0]));
  
  // Iterate the game's preferred languages. First present in (user) wins.
  // If (user) is empty, just return the first token from (game).
  const char *game=0;
  int gamec=pblrt_rom_get_metadata(&game,"lang",4);
  int gamep=0;
  while (gamep<gamec) {
    if ((unsigned char)game[gamep]<=0x20) { gamep++; continue; }
    if (game[gamep]==',') { gamep++; continue; }
    const char *token=game+gamep;
    int tokenc=0;
    while ((gamep<gamec)&&(game[gamep++]!=',')) tokenc++;
    while (tokenc&&((unsigned char)token[tokenc-1]<=0x20)) tokenc--;
    if (tokenc!=2) continue;
    if ((token[0]<'a')||(token[0]>'z')) continue;
    if ((token[1]<'a')||(token[1]>'z')) continue;
    int lang=((token[0]-'a')<<5)|(token[1]-'a');
    if (!userc) return lang; // No user preference; take the first blindly.
    int i=userc; while (i-->0) { // If it matches anywhere in (user), take it.
      if (user[i]==lang) return lang;
    }
  }
  
  // If we reach this, the game does not express a preference.
  // We could double-check by looking at string resource IDs. Maybe they just forgot to declare "lang" in metadata.
  // But that's silly, we should enforce that kind of thing when building the ROM, not when loading it.
  if (userc>=1) return user[0];
  
  // And if we reach this, the user doesn't either.
  // Am I the only one around here that cares about language?
  // Call it English because it has to be something.
  return (('e'-'a')<<5)|('n'-'a');
}
