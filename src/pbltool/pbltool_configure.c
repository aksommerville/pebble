#include "pbltool_internal.h"

/* --help
 */
 
static void pbltool_help_pack() {
  fprintf(stderr,"\nUsage: %s pack -oROM [INPUT...]\n\n",pbltool.exename);
  fprintf(stderr,
    "Produce a ROM file from loose resources.\n"
    "INPUT is usually a directory containing loose resource files.\n"
    "May also be individual files if their names are comprehensible.\n"
    "May also be ROM files to repack.\n"
    "Files later in the command line quietly override those earlier.\n"
    "ID conflicts within one command-line argument are an error.\n"
    "\n"
  );
}
 
static void pbltool_help_unpack() {
  fprintf(stderr,"\nUsage: %s unpack -oDIRECTORY ROM [--raw]\n\n",pbltool.exename);
  fprintf(stderr,
    "Extract all resources from ROM into a new directory.\n"
    "If --raw, we emit the resources exactly as stored.\n"
    "Otherwise, we may attempt to convert to portable formats, depending on resource type.\n"
    "\n"
  );
}
 
static void pbltool_help_bundle() {
  fprintf(stderr,"\nUsage: %s bundle -oEXE|HTML ROM [LIB|--recompile] [--template=HTML]\n\n",pbltool.exename);
  fprintf(stderr,
    "Produce a self-contained executable or web app from a ROM file.\n"
    "\n"
    "For HTML output:\n"
    "Output path must end '.html' or '.htm'.\n"
    "You may supply a template. If not, we use {PEBBLE}/src/web/index.html.\n"
    "Writing your own template is a big deal, it must contain the entire Pebble Runtime.\n"
    "If the template contains <link rel=\"icon\"> with no 'href', we insert the ROM's iconImage# and remove it from the ROM.\n"
    "Any other explicit favicon link is left in place. Beware that the ROM loader will replace favicon if iconImage# is present.\n"
    "Include a <title> tag with dummy content, we'll replace with title@ from the ROM.\n"
    "\n"
    "Fake-native executables:\n"
    "These will include a WebAssembly runtime, and run the ROM exactly as the external native loader does.\n"
    "Do not provide LIB or --recompile.\n"
    "\n"
    "True-native executables:\n"
    "These contain game code compiled for one specific architecture.\n"
    "The ROM is embedded as in the fake-native case, but we remove its 'code' resource.\n"
    "Best to compile the game yourself into a static library and supply that as LIB.\n"
    "Alternately, with --recompile, we will try to turn the ROM's WebAssembly code into native code.\n"
    "\n"
  );
}
 
static void pbltool_help_unbundle() {
  fprintf(stderr,"\nUsage: %s unbundle -oROM EXE|HTML\n\n",pbltool.exename);
  //TODO
}
 
static void pbltool_help_list() {
  fprintf(stderr,"\nUsage: %s list ROM [-fFORMAT]\n\n",pbltool.exename);
  //TODO
}
 
void pbltool_print_usage(const char *topic) {
  if (topic) {
    #define _(tag) if (!strcmp(topic,#tag)) { pbltool_help_##tag(); return; }
    _(pack)
    _(unpack)
    _(bundle)
    _(unbundle)
    _(list)
    #undef _
  }
  fprintf(stderr,"\nUsage: %s COMMAND [OPTIONS]\n\n",pbltool.exename);
  fprintf(stderr,
    "COMMAND:\n"
    "      pack -oROM [INPUT...]\n"
    "    unpack -oDIRECTORY ROM [--raw]\n"
    "    bundle -oEXE|HTML ROM [LIB|--recompile] [--template=HTML]\n"
    "  unbundle -oROM EXE|HTML\n"
    "      list ROM [-fFORMAT]\n"
    "\n"
  );
  fprintf(stderr,"Try `%s --help=COMMAND` for details.\n\n",pbltool.exename);
}

/* Append to srcpathv.
 */
 
static int pbltool_srcpathv_append(const char *v) {
  if (pbltool.srcpathc>=pbltool.srcpatha) {
    int na=pbltool.srcpatha+16;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(pbltool.srcpathv,sizeof(void*)*na);
    if (!nv) return -1;
    pbltool.srcpathv=nv;
    pbltool.srcpatha=na;
  }
  pbltool.srcpathv[pbltool.srcpathc++]=v;
  return 0;
}

/* Receive key=value option.
 */
 
static int pbltool_configure_kv(const char *k,int kc,const char *v,int vc) {
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
  int vn=0;
  sr_int_eval(&vn,v,vc);
  
  if ((kc==4)&&!memcmp(k,"help",4)) {
    pbltool.command="help";
    pbltool_print_usage(v);
    return 0;
  }
  
  if ((kc==1)&&!memcmp(k,"o",1)) {
    pbltool.dstpath=v;
    return 0;
  }
  
  if ((kc==3)&&!memcmp(k,"raw",3)) {
    pbltool.raw=vn;
    return 0;
  }
  
  if ((kc==9)&&!memcmp(k,"recompile",9)) {
    pbltool.recompile=vn;
    return 0;
  }
  
  if ((kc==8)&&!memcmp(k,"template",8)) {
    pbltool.template=v;
    return 0;
  }
  
  //TODO Options
  
  fprintf(stderr,"%s: Unexpected option '%.*s' = '%.*s'\n",pbltool.exename,kc,k,vc,v);
  return -2;
}

/* Receive argv.
 */
 
static int pbltool_configure_argv(int argc,char **argv) {
  int argi=1,err;
  while (argi<argc) { 
    const char *arg=argv[argi++];
    if (!arg||!arg[0]) continue;
    
    if (arg[0]!='-') {
      if (!pbltool.command) {
        pbltool.command=arg;
      } else {
        if ((err=pbltool_srcpathv_append(arg))<0) return err;
      }
      continue;
    }
    
    if (!arg[1]) goto _unexpected_;
    
    if (arg[1]!='-') {
      const char *v=0;
      if (arg[2]) v=arg+2;
      else if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) v=argv[argi++];
      if ((err=pbltool_configure_kv(arg+1,1,v,-1))<0) return err;
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
      if ((err=pbltool_configure_kv(k,kc,v,-1))<0) return err;
      continue;
    }
    
   _unexpected_:;
    fprintf(stderr,"%s: Unexpected argument '%s'\n",pbltool.exename,arg);
    return -2;
  }
  return 0;
}

/* Configure, main.
 */
 
int pbltool_configure(int argc,char **argv) {
  int err;
  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) pbltool.exename=argv[0];
  else pbltool.exename="pbltool";
  if ((err=pbltool_configure_argv(argc,argv))<0) return err;
  return 0;
}
