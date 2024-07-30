#include "pbltool_internal.h"

/* Gather inputs.
 */
 
static int pbltool_pack_gather(struct pbltool_rom *rom) {
  int err,i=0;
  for (;i<pbltool.srcpathc;i++) {
    const char *path=pbltool.srcpathv[i];
    if ((err=pbltool_rom_add_path(rom,path))<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error adding files to rom.\n",path);
      return -2;
    }
  }
  return 0;
}

/* Digest inputs.
 */
 
static int pbltool_pack_digest(struct pbltool_rom *rom) {
  struct pbltool_res *res=rom->resv;
  int i=rom->resc;
  for (;i-->0;res++) {
    if (!res->serialc) continue; // Ignore anything empty.
    if (pbltool_res_is_verbatim(res)) continue; // Preserve anything marked for it.
    int err=0;
    switch (res->tid) {
      case PBL_TID_metadata: err=pbltool_compile_metadata(res); break;
      case PBL_TID_code: break; // code is always verbatim.
      case PBL_TID_strings: err=pbltool_compile_strings(res); break;
      case PBL_TID_image: err=pbltool_compile_image(res); break;
      default: {
          //TODO I'd like to have some mechanism for calling out to user-defined compile tools.
        }
    }
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error compiling resource.\n",res->path);
      return -2;
    }
  }
  return 0;
}

/* pack, main.
 */
 
int pbltool_main_pack() {
  if (!pbltool.dstpath) {
    fprintf(stderr,"%s: Please specify output path as '-oPATH'\n",pbltool.exename);
    return -2;
  }
  struct pbltool_rom rom={0};
  int err=pbltool_pack_gather(&rom);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error gathering rom inputs.\n",pbltool.exename);
    pbltool_rom_cleanup(&rom);
    return -2;
  }
  if ((err=pbltool_pack_digest(&rom))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error digesting rom.\n",pbltool.exename);
    pbltool_rom_cleanup(&rom);
    return -2;
  }
  struct sr_encoder dst={0};
  if (
    ((err=pbltool_rom_validate(&rom))<0)||
    ((err=pbltool_rom_encode(&dst,&rom))<0)
  ) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error encoding rom.\n",pbltool.exename);
    pbltool_rom_cleanup(&rom);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  pbltool_rom_cleanup(&rom);
  if (file_write(pbltool.dstpath,dst.v,dst.c)<0) {
    fprintf(stderr,"%s: Failed to write file, %d bytes.\n",pbltool.dstpath,dst.c);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  sr_encoder_cleanup(&dst);
  return 0;
}
