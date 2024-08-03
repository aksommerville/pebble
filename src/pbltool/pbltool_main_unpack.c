#include "pbltool_internal.h"

/* Digest rom for unpacking.
 */
 
static int pbltool_unpack_digest(struct pbltool_rom *rom) {
  if (pbltool.raw) return 0;
  struct pbltool_res *res=rom->resv;
  int i=rom->resc;
  for (;i-->0;res++) {
    int err=0;
    switch (res->tid) {
      case PBL_TID_metadata: err=pbltool_uncompile_metadata(res); break;
      case PBL_TID_code: break;
      case PBL_TID_strings: err=pbltool_uncompile_strings(res); break;
      case PBL_TID_image: err=pbltool_uncompile_image(res); break;
      case PBL_TID_song: err=pbltool_uncompile_song(res); break;
      default: {
          //TODO Similar to `pack`, should we allow user conversion hooks?
        }
    }
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error uncompiling resource %d:%d\n",pbltool.srcpathv[0],res->tid,res->rid);
      return -2;
    }
  }
  return 0;
}

/* Save resource set to loose files.
 * If we fail, we may leave partial output in place. Caller should delete it.
 */
 
static int pbltool_unpack_save(const char *path,struct pbltool_rom *rom) {
  if (dir_mkdir(path)<0) {
    fprintf(stderr,"%s: mkdir failed\n",path);
    return -2;
  }
  // Resources are sorted by tid in pbltool_rom.
  // That's lucky for us, since we need them batched by tid too.
  struct pbltool_res *res=rom->resv;
  int i=0,err;
  while (i<rom->resc) {
    int resc=1;
    while ((i+resc<rom->resc)&&(res[0].tid==res[resc].tid)) resc++;
    
    // metadata:1 and code:1 are special, they both go straight in the root.
    // If there's more than one of the type, or the rid is not 1, let it thru for generic processing.
    if (resc==1) {
      if ((res->tid==PBL_TID_metadata)&&(res->rid==1)) {
        char rpath[1024];
        int rpathc=snprintf(rpath,sizeof(rpath),"%s/metadata",path);
        if ((rpathc<1)||(rpathc>=sizeof(rpath))) return -1;
        if (file_write(rpath,res->serial,res->serialc)<0) {
          fprintf(stderr,"%s: Failed to write file, %d bytes, for metadata:1\n",rpath,res->serialc);
          return -2;
        }
        res+=resc;
        i+=resc;
        continue;
      }
      if ((res->tid==PBL_TID_code)&&(res->rid==1)) {
        char rpath[1024];
        int rpathc=snprintf(rpath,sizeof(rpath),"%s/code.wasm",path);
        if ((rpathc<1)||(rpathc>=sizeof(rpath))) return -1;
        if (file_write(rpath,res->serial,res->serialc)<0) {
          fprintf(stderr,"%s: Failed to write file, %d bytes, for code:1\n",rpath,res->serialc);
          return -2;
        }
        res+=resc;
        i+=resc;
        continue;
      }
    }
    
    // Make a directory for the batch.
    char dname[256];
    int dnamec=pbltool_tid_repr(dname,sizeof(dname),res->tid);
    if ((dnamec<1)||(dnamec>=sizeof(dname))) return -1;
    char dpath[1024];
    int dpathc=snprintf(dpath,sizeof(dpath),"%s/%.*s",path,dnamec,dname);
    if ((dpathc<1)||(dpathc>=sizeof(dpath))) return -1;
    if (dir_mkdir(dpath)<0) {
      fprintf(stderr,"%s: mkdir failed\n",path);
      return -2;
    }
    
    // Save each resource.
    const struct pbltool_res *subres=res;
    int subi=resc;
    for (;subi-->0;subres++) {
      int rpathc=dpathc;
      dpath[rpathc++]='/';
      err=pbltool_res_compose_base(dpath+rpathc,sizeof(dpath)-rpathc,subres);
      if ((err<1)||(rpathc>=sizeof(dpath)-err)) return -1;
      rpathc+=err;
      dpath[rpathc]=0;
      if (file_write(dpath,subres->serial,subres->serialc)<0) {
        fprintf(stderr,"%s: Failed to write file, %d bytes, for %d:%d\n",dpath,subres->serialc,subres->tid,subres->rid);
        return -2;
      }
    }
    
    res+=resc;
    i+=resc;
  }
  return 0;
}

/* unpack, main.
 */
 
int pbltool_main_unpack() {
  int err;
  if (!pbltool.dstpath) {
    fprintf(stderr,"%s: Please specify output path as '-oPATH', it's a directory that must not exist yet.\n",pbltool.exename);
    return -2;
  }
  if (pbltool.srcpathc!=1) {
    fprintf(stderr,"%s: Exactly one input path required for 'unpack' (a ROM file)\n",pbltool.exename);
    return -2;
  }
  if (file_get_type(pbltool.dstpath)) {
    fprintf(stderr,"%s: File already exists.\n",pbltool.dstpath);
    return -2;
  }
  
  // Funny thing about this is you could also provide unpacked roms, in which case we're just copying files for you.
  // I'm not documenting that fact, and not guaranteeing that it will always work that way.
  struct pbltool_rom rom={0};
  if ((err=pbltool_rom_add_path(&rom,pbltool.srcpathv[0]))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error decoding rom file.\n",pbltool.srcpathv[0]);
    pbltool_rom_cleanup(&rom);
    return -2;
  }
  
  if ((err=pbltool_unpack_digest(&rom))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error decompiling rom resources.\n",pbltool.dstpath);
    pbltool_rom_cleanup(&rom);
    return -2;
  }
  
  if ((err=pbltool_unpack_save(pbltool.dstpath,&rom))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error saving unpacked rom.\n",pbltool.dstpath);
    pbltool_rom_cleanup(&rom);
    dir_rmrf(pbltool.dstpath);
    return -2;
  }
  
  pbltool_rom_cleanup(&rom);
  return 0;
}
