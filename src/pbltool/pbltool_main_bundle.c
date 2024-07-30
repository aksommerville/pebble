#include "pbltool_internal.h"

/* bundle HTML.
 */
 
static int pbltool_bundle_html(struct sr_encoder *dst,struct pbltool_rom *rom,const char *rompath) {
  if (sr_encode_raw(dst,
    "<!DOCTYPE html>\n"
    "<html><head>\n"
    "<title>Pebble Game</title>\n"
    "</head><body>\n"
    "<p>TODO: Bundle ROM to HTML</p>\n"
    "</body></html>\n"
  ,-1)<0) return -1;
  return 0;
}

/* bundle true-native with a library of the game's code.
 */
 
static int pbltool_bundle_true_lib(struct sr_encoder *dst,struct pbltool_rom *rom,const char *rompath,const char *libpath) {
  fprintf(stderr,"%s TODO %s\n",__func__,rompath);
  return -2;
}

/* bundle true-native by recompiling code:1 to the host architecture.
 */
 
static int pbltool_bundle_true(struct sr_encoder *dst,struct pbltool_rom *rom,const char *rompath) {
  fprintf(stderr,"%s TODO %s\n",__func__,rompath);
  return -2;
}

/* bundle fake-native.
 */
 
static int pbltool_bundle_fake(struct sr_encoder *dst,struct pbltool_rom *rom,const char *rompath) {
  fprintf(stderr,"%s TODO %s\n",__func__,rompath);
  return -2;
}

/* bundle, main.
 */
 
int pbltool_main_bundle() {
  if (!pbltool.dstpath) {
    fprintf(stderr,"%s: Please specify output path as '-oPATH'\n",pbltool.exename);
    return -2;
  }
  if (pbltool.srcpathc<1) {
    fprintf(stderr,"%s: Input path required.\n",pbltool.exename);
    return -2;
  }
  if (pbltool.srcpathc>2) {
    fprintf(stderr,"%s: Unexpected extra inputs.\n",pbltool.exename);
    return -2;
  }
  const char *rompath=pbltool.srcpathv[0];
  const char *libpath=0;
  if (pbltool.srcpathc>=2) libpath=pbltool.srcpathv[1];
  int dstpathc=0;
  while (pbltool.dstpath[dstpathc]) dstpathc++;
  
  int err=-1;
  struct sr_encoder dst={0};
  struct pbltool_rom rom={0};
  if ((err=pbltool_rom_add_path(&rom,rompath))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error decoding ROM file.\n",rompath);
    pbltool_rom_cleanup(&rom);
    return -2;
  }
  
  //TODO The executable cases might not produce a binary in memory; they might invoke ld or something.
  //...in fact, they might not benefit from a decoded ROM either.
  // This might need heavy restructuring, once we're ready to finish implementation.
  if (
    ((dstpathc>=5)&&!memcmp(pbltool.dstpath+dstpathc-5,".html",5))||
    ((dstpathc>=4)&&!memcmp(pbltool.dstpath+dstpathc-4,".htm",4))
  ) {
    err=pbltool_bundle_html(&dst,&rom,rompath);
  } else if (libpath) {
    err=pbltool_bundle_true_lib(&dst,&rom,rompath,libpath);
  } else if (pbltool.recompile) {
    err=pbltool_bundle_true(&dst,&rom,rompath);
  } else {
    err=pbltool_bundle_fake(&dst,&rom,rompath);
  }
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error producing bundle.\n",rompath);
    sr_encoder_cleanup(&dst);
    pbltool_rom_cleanup(&rom);
    return -2;
  }
  
  if (file_write(pbltool.dstpath,dst.v,dst.c)<0) {
    fprintf(stderr,"%s: Failed to write file, %d bytes.\n",pbltool.dstpath,dst.c);
    sr_encoder_cleanup(&dst);
    pbltool_rom_cleanup(&rom);
    return -2;
  }
  
  sr_encoder_cleanup(&dst);
  pbltool_rom_cleanup(&rom);
  return 0;
}
