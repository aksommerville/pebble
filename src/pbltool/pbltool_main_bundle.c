#include "pbltool_internal.h"

int pbltool_bundle_html(struct sr_encoder *dst,struct pbltool_rom *rom,const char *rompath);
int pbltool_bundle_true_lib(const char *dstpath,struct pbltool_rom *rom,const char *rompath,const char *libpath);
int pbltool_bundle_true_recompile(const char *dstpath,struct pbltool_rom *rom,const char *rompath);
int pbltool_bundle_fake(const char *dstpath,const char *rompath);

/* bundle, main.
 */
 
int pbltool_main_bundle() {

  /* Digest request.
   */
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
  int dstpathc=0,err;
  while (pbltool.dstpath[dstpathc]) dstpathc++;
  
  #define NEEDROM \
    struct pbltool_rom rom={0}; \
    if ((err=pbltool_rom_add_path(&rom,rompath))<0) { \
      if (err!=-2) fprintf(stderr,"%s: Unspecified error decoding ROM file.\n",rompath); \
      pbltool_rom_cleanup(&rom); \
      return -2; \
    }
  
  /* Output path ends ".html", we're doing HTML.
   */
  if (
    ((dstpathc>=5)&&!memcmp(pbltool.dstpath+dstpathc-5,".html",5))||
    ((dstpathc>=4)&&!memcmp(pbltool.dstpath+dstpathc-4,".htm",4))
  ) {
    if (libpath) fprintf(stderr,"%s:WARNING: Library '%s' is ignored for HTML output.\n",pbltool.exename,libpath);
    if (pbltool.recompile) fprintf(stderr,"%s:WARNING: '--recompile' is ignored for HTML output.\n",pbltool.exename);
    struct sr_encoder dst={0};
    NEEDROM
    err=pbltool_bundle_html(&dst,&rom,rompath);
    pbltool_rom_cleanup(&rom);
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Bundling HTML failed.\n",rompath);
      sr_encoder_cleanup(&dst);
      return -2;
    }
    if (file_write(pbltool.dstpath,dst.v,dst.c)<0) {
      fprintf(stderr,"%s: Failed to write file, %d bytes.\n",pbltool.dstpath,dst.c);
      sr_encoder_cleanup(&dst);
      return -2;
    }
    sr_encoder_cleanup(&dst);
    return 0;
  }
  
  /* If a library was provided, we're doing true-native with it.
   */
  if (libpath) {
    if (pbltool.recompile) fprintf(stderr,"%s:WARNING: '--recompile' is ignored due to library '%s'\n",pbltool.exename,libpath);
    NEEDROM
    err=pbltool_bundle_true_lib(pbltool.dstpath,&rom,rompath,libpath);
    pbltool_rom_cleanup(&rom);
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error generating true-native bundle with library.\n",rompath);
      return -2;
    }
    return 0;
  }
  
  /* With --recompile, it's true-native with WebAssembly=>Native recompilation.
   */
  if (pbltool.recompile) {
    NEEDROM
    err=pbltool_bundle_true_recompile(pbltool.dstpath,&rom,rompath);
    pbltool_rom_cleanup(&rom);
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error generating true-native bundle by recompilation.\n",rompath);
      return -2;
    }
    return 0;
  }
  
  /* If nothing else, it's fake-native.
   */
  if ((err=pbltool_bundle_fake(pbltool.dstpath,rompath))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error generating fake-native bundle.\n",rompath);
    return -2;
  }
  return 0;
  #undef NEEDROM
}
