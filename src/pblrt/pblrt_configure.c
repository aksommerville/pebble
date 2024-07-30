#include "pblrt_internal.h"

/* --help
 */
 
void pblrt_print_usage() {
  fprintf(stderr,"\nUsage: %s ROM [OPTIONS]\n\n",pblrt.exename);
  fprintf(stderr,
    "Usually omitting all options yields sensible defaults:\n"
    "  --help                     Print this message and exit.\n"
    "\n"
  );
  //TODO Driver options, and dynamically list drivers.
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
  
  if ((kc==4)&&!memcmp(k,"help",4)) {
    pblrt.terminate=1;
    pblrt_print_usage();
    return 0;
  }
  
  //TODO Options
  
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
      //TODO Jump right to _unexpected_ if we have an embedded ROM.
      if (!pblrt.rompath) {
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

/* Configure, main.
 */
 
int pblrt_configure(int argc,char **argv) {
  int err;
  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) pblrt.exename=argv[0];
  else pblrt.exename="pblrt";
  if ((err=pblrt_configure_argv(argc,argv))<0) return err;
  return 0;
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
