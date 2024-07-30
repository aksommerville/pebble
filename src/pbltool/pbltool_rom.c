#include "pbltool_internal.h"

/* Cleanup.
 */

static void pbltool_res_cleanup(struct pbltool_res *res) {
  if (res->path) free(res->path);
  if (res->name) free(res->name);
  if (res->comment) free(res->comment);
  if (res->format) free(res->format);
  if (res->serial) free(res->serial);
}

void pbltool_rom_cleanup(struct pbltool_rom *rom) {
  if (rom->resv) {
    while (rom->resc-->0) pbltool_res_cleanup(rom->resv+rom->resc);
    free(rom->resv);
  }
  memset(rom,0,sizeof(struct pbltool_rom));
}

/* IDs from path.
 */
 
int pbltool_rom_resolve_ids(struct pbltool_res *tmp,struct pbltool_rom *rom,const char *path,int pathc) {
  if (!tmp||!path) return -1;
  if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  
  memset(tmp,0,sizeof(struct pbltool_res));
  tmp->path=(char*)path;
  tmp->pathc=pathc;
  
  // Take the last two components.
  const char *dir="",*base=path;
  int dirc=0,basec=0,pathp=0;
  for (;pathp<pathc;pathp++) {
    if (path[pathp]=='/') {
      dir=base;
      dirc=basec;
      base=path+pathp+1;
      basec=0;
    } else {
      basec++;
    }
  }
  
  // "metadata" and "code.wasm" are special.
  if ((basec==8)&&!memcmp(base,"metadata",8)) {
    tmp->tid=PBL_TID_metadata;
    tmp->rid=1;
    return 0;
  }
  if ((basec==9)&&!memcmp(base,"code.wasm",9)) {
    tmp->tid=PBL_TID_code;
    tmp->rid=1;
    return 0;
  }
  
  // If (dir) begins with a decimal integer, that is (tid).
  if ((dirc>=1)&&(dir[0]>='0')&&(dir[0]<='9')) {
    int dirp=0;
    while ((dirp<dirc)&&(dir[dirp]>='0')&&(dir[dirp]<='9')) {
      tmp->tid*=10;
      tmp->tid+=dir[dirp++]-'0';
      if (tmp->tid>0x3f) return -1;
    }
    if ((dirp<dirc)&&(dir[dirp]!='-')) return -1;
  // No explicit (tid), it must be a standard type.
  } else {
    if ((tmp->tid=pbltool_tid_eval(dir,dirc))<0) return -1;
  }
  
  // Basename is: [LANG-]RID[-NAME][[.COMMENT].FORMAT]
  int basep=0,lang=0;
  if ((basec>=3)&&(base[0]>='a')&&(base[0]<='z')&&(base[1]>='a')&&(base[1]<='z')&&(base[2]=='-')) {
    lang=((base[0]-'a')<<11)|((base[1]-'a')<<6);
    basep=3;
  }
  // RID is required.
  if ((basep>=basec)||(base[basep]<'0')||(base[basep]>'9')) return -1;
  while ((basep<basec)&&(base[basep]>='0')&&(base[basep]<='9')) {
    tmp->rid*=10;
    tmp->rid+=base[basep++]-'0';
    if (tmp->rid>0xffff) return -1;
  }
  if (!tmp->rid) return -1;
  if (lang) {
    if (tmp->rid>0x3f) return -1;
    tmp->rid|=lang;
  }
  // NAME optional.
  if ((basep<basec)&&(base[basep]=='-')) {
    basep++;
    tmp->name=(char*)(base+basep);
    while ((basep<basec)&&(base[basep]!='.')) {
      tmp->namec++;
      basep++;
    }
  }
  // COMMENT and FORMAT optional, and a little tricky to tease apart.
  // Read the whole thing as FORMAT first, then split on the last dot of that.
  if ((basep<basec)&&(base[basep]=='.')) {
    basep++;
    tmp->format=(char*)(base+basep);
    tmp->formatc=basec-basep;
    basep=basec;
    int i=tmp->formatc;
    while (i-->0) {
      if (tmp->format[i]=='.') {
        tmp->comment=tmp->format;
        tmp->commentc=i;
        tmp->format=tmp->format+i+1;
        tmp->formatc=tmp->formatc-i-1;
        break;
      }
    }
  }
  
  if (basep<basec) return -1;
  return 0;
}

/* Add resource file.
 * Hands off (src) on success.
 */
 
static int pbltool_rom_handoff_resfile(struct pbltool_rom *rom,void *src,int srcc,const char *path) {
  
  // Resolve IDs into a temporary record.
  struct pbltool_res tmp={0};
  int err=pbltool_rom_resolve_ids(&tmp,rom,path,-1);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Failed to parse res file path.\n",path);
    return -2;
  }
  if ((tmp.tid<1)||(tmp.tid>0x3f)||(tmp.rid<1)||(tmp.rid>0xffff)) {
    fprintf(stderr,"%s: Invalid resource IDs.\n",path);
    return -2;
  }
  
  // Add resource.
  struct pbltool_res *res=0;
  int p=pbltool_rom_search(rom,tmp.tid,tmp.rid);
  if (p<0) {
    p=-p-1;
    if (!(res=pbltool_rom_add_res(rom,p,tmp.tid,tmp.rid))) return -1;
  } else {
    res=rom->resv+p;
    if (res->seq==rom->seq) {
      fprintf(stderr,"%s: Resource ID %d:%d conflicts with %s\n",path,tmp.tid,tmp.rid,res->path);
      return -2;
    }
  }
  if (pbltool_res_set_path(res,tmp.path,tmp.pathc)<0) return -1;
  if (pbltool_res_set_name(res,tmp.name,tmp.namec)<0) return -1;
  if (pbltool_res_set_comment(res,tmp.comment,tmp.commentc)<0) return -1;
  if (pbltool_res_set_format(res,tmp.format,tmp.formatc)<0) return -1;
  pbltool_res_handoff_serial(res,src,srcc);
  
  return 0;
}

/* Add directory, top level.
 */

static int pbltool_rom_add_directory(struct pbltool_rom *rom,const char *path);

static int pbltool_rom_add_dir_cb(const char *path,const char *base,char ftype,void *userdata) {
  struct pbltool_rom *rom=userdata;
  if (!ftype) ftype=file_get_type(path);
  if (ftype=='d') return pbltool_rom_add_directory(rom,path);
  if (ftype=='f') {
    void *serial=0;
    int serialc=file_read(&serial,path);
    if (serialc<0) {
      fprintf(stderr,"%s: Failed to read file\n",path);
      return -2;
    }
    int err=pbltool_rom_handoff_resfile(rom,serial,serialc,path);
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error adding resource file\n",path);
      free(serial);
      return -2;
    }
    return 0;
  }
  return 0;
}
 
static int pbltool_rom_add_directory(struct pbltool_rom *rom,const char *path) {
  return dir_read(path,pbltool_rom_add_dir_cb,rom);
}

/* Add ROM file.
 */
 
static int pbltool_rom_add_archive(struct pbltool_rom *rom,const uint8_t *src,int srcc,const char *path) {
  if ((srcc<8)||memcmp(src,"\x00\xffPBLROM",8)) return -1;
  int srcp=8,tid=1,rid=1,err;
  while (srcp<srcc) {
    uint8_t lead=src[srcp++];
    if (!lead) break;
    int len=0;
    switch (lead&0xc0) {
      case 0x00: {
          tid+=lead;
          if (tid>0x3f) return -1;
          rid=1;
        } break;
      case 0x40: {
          if (srcp>srcc-1) return -1;
          int d=((lead&0x3f)<<8)|src[srcp++];
          rid+=d;
          if (rid>0xffff) return -1;
        } break;
      case 0x80: {
          len=(lead&0x3f)+1;
        } break;
      case 0xc0: {
          if (srcp>srcc-2) return -1;
          len=((lead&0x3f)<<16)|(src[srcp]<<8)|src[srcp+1];
          len+=65;
          srcp+=2;
        } break;
    }
    if (len) {
      if (srcp>srcc-len) return -1;
      if (rid>0xffff) return -1;
      // Runtime decoders can assume the new one goes at the end.
      // We can't, because we support multiple sources.
      struct pbltool_res *res;
      int p=pbltool_rom_search(rom,tid,rid);
      if (p<0) {
        p=-p-1;
        if (!(res=pbltool_rom_add_res(rom,p,tid,rid))) return -1;
      } else {
        res=rom->resv+p;
        if (res->seq==rom->seq) return -1; // Possible if some other source failed to increment (rom->seq).
        res->seq=rom->seq;
      }
      if (pbltool_res_set_serial(res,src+srcp,len)<0) return -1;
      srcp+=len;
    }
  }
  return 0;
}

/* Add file, directory, or archive.
 */

int pbltool_rom_add_path(struct pbltool_rom *rom,const char *path) {
  if (!path||!path[0]) return -1;
  char ftype=file_get_type(path);
  if (ftype=='d') {
    int err=pbltool_rom_add_directory(rom,path);
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error adding directory.\n",path);
      return -2;
    }
  } else if (ftype=='f') {
    void *serial=0;
    int serialc=file_read(&serial,path);
    if (serialc<0) {
      fprintf(stderr,"%s: Failed to read file.\n",path);
      return -2;
    }
    if ((serialc>=8)&&!memcmp(serial,"\x00\xffPBLROM",8)) {
      int err=pbltool_rom_add_archive(rom,serial,serialc,path);
      free(serial);
      if (err<0) {
        if (err!=-2) fprintf(stderr,"%s: Unspecified error decoding ROM file.\n",path);
        return -2;
      }
    } else {
      int err=pbltool_rom_handoff_resfile(rom,serial,serialc,path);
      if (err<0) {
        if (err!=-2) fprintf(stderr,"%s: Unspecified error adding resource file.\n",path);
        free(serial);
        return -2;
      }
      // (serial) handed off
    }
  } else {
    fprintf(stderr,"%s: Unexpected file type '%c'\n",path,ftype);
    return -2;
  }
  rom->seq++;
  return 0;
}

/* Search resources.
 */

int pbltool_rom_search(const struct pbltool_rom *rom,int tid,int rid) {
  int lo=0,hi=rom->resc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct pbltool_res *q=rom->resv+ck;
         if (tid<q->tid) hi=ck;
    else if (tid>q->tid) lo=ck+1;
    else if (rid<q->rid) hi=ck;
    else if (rid>q->rid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Add resource to list.
 */

struct pbltool_res *pbltool_rom_add_res(struct pbltool_rom *rom,int p,int tid,int rid) {
  if ((p<0)||(p>rom->resc)) return 0;
  if (p) {
    const struct pbltool_res *q=rom->resv+p-1;
    if (tid<q->tid) return 0;
    if ((tid==q->tid)&&(rid<=q->rid)) return 0;
  }
  if (p<rom->resc) {
    const struct pbltool_res *q=rom->resv+p;
    if (tid>q->tid) return 0;
    if ((tid==q->tid)&&(rid>=q->rid)) return 0;
  }
  if (rom->resc>=rom->resa) {
    int na=rom->resa+128;
    if (na>INT_MAX/sizeof(struct pbltool_res)) return 0;
    void *nv=realloc(rom->resv,sizeof(struct pbltool_res)*na);
    if (!nv) return 0;
    rom->resv=nv;
    rom->resa=na;
  }
  struct pbltool_res *res=rom->resv+p;
  memmove(res+1,res,sizeof(struct pbltool_res)*(rom->resc-p));
  memset(res,0,sizeof(struct pbltool_res));
  rom->resc++;
  res->tid=tid;
  res->rid=rid;
  res->seq=rom->seq;
  return res;
}

/* Resource record accessors.
 */
 
#define SETSTRING(name) \
  int pbltool_res_set_##name(struct pbltool_res *res,const char *src,int srcc) { \
    if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; } \
    char *nv=0; \
    if (srcc) { \
      if (!(nv=malloc(srcc+1))) return -1; \
      memcpy(nv,src,srcc); \
      nv[srcc]=0; \
    } \
    if (res->name) free(res->name); \
    res->name=nv; \
    res->namec=srcc; \
    return 0; \
  }
SETSTRING(path)
SETSTRING(name)
SETSTRING(comment)
SETSTRING(format)
#undef SETSTRING

int pbltool_res_set_serial(struct pbltool_res *res,const void *src,int srcc) {
  if ((srcc<0)||(srcc&&!src)) return -1;
  void *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc))) return -1;
    memcpy(nv,src,srcc);
  }
  if (res->serial) free(res->serial);
  res->serial=nv;
  res->serialc=srcc;
  return 0;
}

void pbltool_res_handoff_serial(struct pbltool_res *res,void *src,int srcc) {
  if ((srcc<0)||(srcc&&!src)) return;
  if (res->serial) free(res->serial);
  res->serial=src;
  res->serialc=srcc;
}

/* Detect resources flagged for verbatim output.
 */
 
int pbltool_res_is_verbatim(const struct pbltool_res *res) {
  if (!res) return 0;
  if ((res->formatc==3)&&!memcmp(res->format,"raw",3)) return 1;
  const char *src=res->comment;
  int srcc=res->commentc,srcp=0;
  while (srcp<srcc) {
    if (src[srcp]=='.') { srcp++; continue; }
    const char *token=src+srcp;
    int tokenc=1;
    while ((srcp<srcc)&&(src[srcp++]!='.')) tokenc++;
    if ((tokenc==3)&&!memcmp(token,"raw",3)) return 1;
  }
  return 0;
}

/* Generate basename.
 */
 
int pbltool_res_compose_base(char *dst,int dsta,const struct pbltool_res *res) {
  int dstc=0,err;
  int rid=res->rid;
  
  // strings are special. The top ten bits of rid are treated as a language code.
  // We'll validate that it does look like a real language. eg we will not produce "aa" for low rids.
  if ((res->tid==PBL_TID_strings)&&(res->rid&0x3f)&&(res->rid&0xffc0)&&!(res->rid&~0xffff)) {
    int hi=(res->rid>>11)&0x1f;
    int lo=(res->rid>>5)&0x1f;
    if ((hi<26)&&(lo<26)) {
      if (dstc<=dsta-2) {
        dst[dstc++]="abcdefghijklmnopqrstuvwxyz"[hi];
        dst[dstc++]="abcdefghijklmnopqrstuvwxyz"[lo];
      } else dstc+=2;
      if (dstc<dsta) dst[dstc]='-';
      dstc++;
      rid&=0x3f;
    }
  }
  
  // Decimal rid.
  if ((err=sr_decsint_repr(dst+dstc,dsta-dstc,res->rid))<0) return err;
  dstc+=err;
  
  // If there's a name, append it with a dash introducer.
  if (res->namec) {
    if (dstc<dsta) dst[dstc]='-';
    dstc++;
    if (dstc<=dsta-res->namec) memcpy(dst+dstc,res->name,res->namec);
    dstc+=res->namec;
  }
  
  // If there's a comment, append it with a dot introducer.
  if (res->commentc) {
    if (dstc<dsta) dst[dstc]='.';
    dstc++;
    if (dstc<=dsta-res->commentc) memcpy(dst+dstc,res->comment,res->commentc);
    dstc+=res->commentc;
  }
  
  // If there's a format, append it with a dot introducer.
  if (res->formatc) {
    if (dstc<dsta) dst[dstc]='.';
    dstc++;
    if (dstc<=dsta-res->formatc) memcpy(dst+dstc,res->format,res->formatc);
    dstc+=res->formatc;
  }
  
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Clean up and validate rom.
 */

int pbltool_rom_validate(struct pbltool_rom *rom) {
  // First remove empties:
  struct pbltool_res *res=rom->resv+rom->resc-1;
  int i=rom->resc;
  for (;i-->0;res--) {
    if (res->serialc) continue;
    pbltool_res_cleanup(res);
    rom->resc--;
    memmove(res,res+1,sizeof(struct pbltool_res)*(rom->resc-i));
  }
  // Then validate everything we can:
  int tid=1,rid=1;
  for (res=rom->resv,i=0;i<rom->resc;i++,res++) {
    if ((res->tid<tid)||((res->tid==tid)&&(res->rid<rid))) {
      fprintf(stderr,"Resource %d:%d out of order, expected at least %d:%d\n",res->tid,res->rid,tid,rid);
      return -2;
    }
    tid=res->tid;
    rid=res->rid;
    if ((res->tid>0x3f)||(res->rid>0xffff)) {
      fprintf(stderr,"Invalid resource id %d:%d, limit 63:65535\n",res->tid,res->rid);
      return -2;
    }
    if (res->serialc>4194368) {
      fprintf(stderr,"Invalid size %d for resource %d:%d (%s), limit %d\n",res->serialc,res->tid,res->rid,res->path,4194368);
      return -2;
    }
  }
  return 0;
}

/* Encode ROM file.
 */

int pbltool_rom_encode(struct sr_encoder *dst,const struct pbltool_rom *rom) {
  if (sr_encode_raw(dst,"\x00\xffPBLROM",8)<0) return -1;
  int tid=1,rid=1;
  const struct pbltool_res *res=rom->resv;
  int i=rom->resc;
  for (;i-->0;res++) {

    // Update tid.
    if (res->tid>0x3f) return -1;
    int dtid=res->tid-tid;
    if (dtid<0) return -1;
    if (dtid>0) {
      while (dtid>=0x3f) {
        if (sr_encode_u8(dst,0x3f)<0) return -1;
        dtid-=0x3f;
      }
      if (dtid>0) {
        if (sr_encode_u8(dst,dtid)<0) return -1;
      }
      tid=res->tid;
      rid=1;
    }
    
    // Update rid.
    if (res->rid>0xffff) return -1;
    int drid=res->rid-rid;
    if (drid<0) return -1;
    if (drid>0) {
      while (drid>=0x3fff) {
        if (sr_encode_intbe(dst,0x7fff,2)<0) return -1;
        drid-=0x3fff;
      }
      if (drid>0) {
        if (sr_encode_intbe(dst,0x4000|drid,2)<0) return -1;
      }
      rid=res->rid;
    }
    
    // Emit resource.
    if (res->serialc>4194368) {
      return -1;
    } else if (res->serialc>=65) {
      if (sr_encode_intbe(dst,0xc00000|(res->serialc-65),3)<0) return -1;
    } else if (res->serialc>=1) {
      if (sr_encode_u8(dst,0x80|(res->serialc-1))<0) return -1;
    } else {
      return -1;
    }
    if (sr_encode_raw(dst,res->serial,res->serialc)<0) return -1;
    rid++;
  }
  return 0;
}

/* Conveniences for reading.
 */
 
int pbltool_rom_get(void *dstpp,const struct pbltool_rom *rom,int tid,int rid) {
  int p=pbltool_rom_search(rom,tid,rid);
  if (p<0) return 0;
  *(void**)dstpp=rom->resv[p].serial;
  return rom->resv[p].serialc;
}

int pbltool_rom_get_meta(void *dstpp,const struct pbltool_rom *rom,const char *k,int kc) {
  if (!k) return 0;
  if (kc<0) { kc=0; while (k[kc]) kc++; }
  const uint8_t *src=0;
  int srcc=pbltool_rom_get(&src,rom,PBL_TID_metadata,1);
  if ((srcc<4)||memcmp(src,"\x00PM\xff",4)) return 0;
  int srcp=4;
  for (;;) {
    if (srcp>=srcc) return 0;
    int qkc=src[srcp++];
    if (!qkc) return 0;
    if (srcp>=srcc) return 0;
    int vc=src[srcp++];
    if (srcp>srcc-vc-qkc) return 0;
    if ((qkc==kc)&&!memcmp(src+srcp,k,kc)) {
      srcp+=qkc;
      *(const void**)dstpp=src+srcp;
      return vc;
    }
    srcp+=qkc+vc;
  }
}

int pbltool_rom_get_string(void *dstpp,const struct pbltool_rom *rom,int rid,int index) {
  if (index<0) return 0;
  const uint8_t *src=0;
  int srcc=pbltool_rom_get(&src,rom,PBL_TID_strings,rid);
  if ((srcc<4)||memcmp(src,"\x00PS\xff",4)) return 0;
  int srcp=4;
  for (;;) {
    if (srcp>srcc-2) return 0;
    int len=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (srcp>srcc-len) return 0;
    if (!index--) {
      *(const void**)dstpp=src+srcp;
      return len;
    }
    srcp+=len;
  }
}

void pbltool_rom_clear_resource(struct pbltool_rom *rom,int tid,int rid) {
  int p=pbltool_rom_search(rom,tid,rid);
  if (p>=0) rom->resv[p].serialc=0;
}

/* Remove a field from metadata:1.
 */
 
int pbltool_rom_remove_meta(struct pbltool_rom *rom,const char *k,int kc) {
  if (!k) return -1;
  if (kc<0) { kc=0; while (k[kc]) kc++; }
  int p=pbltool_rom_search(rom,PBL_TID_metadata,1);
  if (p<0) return -1;
  struct pbltool_res *res=rom->resv+p;
  if ((res->serialc<4)||memcmp(res->serial,"\x00PM\xff",4)) return -1;
  uint8_t *src=res->serial;
  int srcp=4;
  while (srcp<res->serialc) {
    int qkc=src[srcp++];
    if (!qkc) return -1;
    if (srcp>=res->serialc) return -1;
    int vc=src[srcp++];
    if (srcp>res->serialc-vc-qkc) return -1;
    if ((qkc==kc)&&!memcmp(k,src+srcp,kc)) {
      srcp-=2;
      int rmc=2+qkc+vc;
      res->serialc-=rmc;
      memmove(src+srcp,src+srcp+rmc,res->serialc-srcp);
      return 0;
    }
    srcp+=qkc+vc;
  }
  return -1;
}
