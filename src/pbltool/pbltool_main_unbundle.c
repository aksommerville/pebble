#include "pbltool_internal.h"

/* Context for unbundling HTML.
 */
 
struct pbltool_unbundle_html {
  const char *srcpath;
  const char *icon; // A "data:" URL or empty, from <link rel="icon">
  int iconc;
  const char *b64; // Inner text of <pbl-rom>. A base64-encoded ROM file.
  int b64c;
  struct pbltool_rom rom; // Only used if we have to repack due to icon.
  struct sr_encoder scratch; // ''
};

static void pbltool_unbundle_html_cleanup(struct pbltool_unbundle_html *ctx) {
  pbltool_rom_cleanup(&ctx->rom);
  sr_encoder_cleanup(&ctx->scratch);
}

/* Decode provided base64 text into the provided encoder.
 */
 
static int pbltool_unbundle_html_decode_base64(struct sr_encoder *dst,const char *src,int srcc,const char *srcpath) {
  int srcp=0;
  uint8_t tmp[4];
  int tmpc=0;
  while (srcp<srcc) {
    char ch=src[srcp++];
    if ((unsigned char)ch<=0x20) continue;
    if (ch=='=') continue;
         if ((ch>='A')&&(ch<='Z')) tmp[tmpc++]=ch-'A';
    else if ((ch>='a')&&(ch<='z')) tmp[tmpc++]=ch-'a'+26;
    else if ((ch>='0')&&(ch<='9')) tmp[tmpc++]=ch-'0'+52;
    else if (ch=='+') tmp[tmpc++]=62;
    else if (ch=='/') tmp[tmpc++]=63;
    else {
      fprintf(stderr,"%s: Unexpected character '%c' in base64 text\n",srcpath,ch);
      return -2;
    }
    if (tmpc>=4) {
      if (
        (sr_encode_u8(dst,(tmp[0]<<2)|(tmp[1]>>4))<0)||
        (sr_encode_u8(dst,(tmp[1]<<4)|(tmp[2]>>2))<0)||
        (sr_encode_u8(dst,(tmp[2]<<6)|tmp[3])<0)
      ) return -1;
      tmpc=0;
    }
  }
  switch (tmpc) {
    case 1: return -1;
    case 2: {
        if (
          (sr_encode_u8(dst,(tmp[0]<<2)|(tmp[1]>>4))<0)
        ) return -1;
      } break;
    case 3: {
        if (
          (sr_encode_u8(dst,(tmp[0]<<2)|(tmp[1]>>4))<0)||
          (sr_encode_u8(dst,(tmp[1]<<4)|(tmp[2]>>2))<0)
        ) return -1;
      } break;
  }
  return 0;
}

/* Populate context (icon,b64) from HTML text.
 * Fails if no <pbl-rom> present.
 */
 
static int pbltool_unbundle_html_scan(struct pbltool_unbundle_html *ctx,const char *src,int srcc,const char *srcpath) {
  int srcp=0;
  while (srcp<srcc) {
  
    // Everything we're interested in is an HTML tag.
    if (src[srcp++]!='<') continue;
    
    // Skip comments.
    if ((srcp<=srcc-3)&&!memcmp(src+srcp,"!--",3)) {
      srcp+=3;
      for (;;srcp++) {
        if (srcp>srcc-3) break;
        if (!memcmp(src+srcp,"-->",3)) {
          srcp+=3;
          break;
        }
      }
      continue;
    }
    
    // Get the tag name.
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
    const char *tagname=src+srcp;
    int tagnamec=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)&&(src[srcp]!='/')&&(src[srcp]!='>')) { srcp++; tagnamec++; }
    
    // <link>. If it contains 'rel="icon"' and href begins "data:", record it.
    if ((tagnamec==4)&&!memcmp(tagname,"link",4)) {
      const char *rel=0,*href=0;
      int relc=0,hrefc=0;
      for (;;) {
        if (((unsigned char)src[srcp]<=0x20)||(src[srcp]=='/')) {
          srcp++;
          continue;
        }
        if (src[srcp]=='>') {
          srcp++;
          break;
        }
        const char *k=src+srcp,*v=0;
        int kc=0,vc=0;
        while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)&&(src[srcp]!='=')&&(src[srcp]!='/')&&(src[srcp]!='>')) {
          srcp++;
          kc++;
        }
        if ((srcp<srcc)&&((unsigned char)src[srcp]=='=')) {
          srcp++;
          if ((srcp<srcc)&&((unsigned char)src[srcp]=='"')) {
            srcp++;
            v=src+srcp;
            while ((srcp<srcc)&&(src[srcp++]!='"')) vc++;
          } else {
            v=src+srcp;
            while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)&&(src[srcp]!='/')&&(src[srcp]!='>')) { srcp++; vc++; }
          }
        }
        if ((kc==3)&&!memcmp(k,"rel",3)) {
          rel=v;
          relc=vc;
        } else if ((kc==4)&&!memcmp(k,"href",4)) {
          href=v;
          hrefc=vc;
        }
      }
      if ((relc==4)&&!memcmp(rel,"icon",4)&&(hrefc>=5)&&!memcmp(href,"data:",5)) {
        ctx->icon=href;
        ctx->iconc=hrefc;
      }
      continue;
    }
    
    // <pbl-rom>: Capture inner text.
    if ((tagnamec==7)&&!memcmp(tagname,"pbl-rom",7)) {
      while ((srcp<srcc)&&(src[srcp++]!='>')) ;
      while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
      ctx->b64=src+srcp;
      ctx->b64c=0;
      while ((srcp<srcc)&&(src[srcp]!='<')) { srcp++; ctx->b64c++; }
      while (ctx->b64c&&((unsigned char)ctx->b64[ctx->b64c-1]<=0x20)) ctx->b64c--;
      continue;
    }
    
    // Any other tag, just skip thru the end of the open tag.
    while ((srcp<srcc)&&(src[srcp++]!='>')) ;
  }
  return 0;
}

/* Add or replace one field in a metadata resource, rewriting it.
 */
 
static int pbltool_unbundle_rewrite_metadata(struct sr_encoder *dst,const uint8_t *src,int srcc,const char *k,int kc,const char *v,int vc) {
  if (!k) return -1;
  if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  if ((kc<1)||(kc>0xff)) return -1;
  if (vc>0xff) return -1;
  if ((srcc<4)||memcmp(src,"\x00PM\xff",4)) return -1;
  if (sr_encode_raw(dst,"\x00PM\xff",4)<0) return -1;
  int srcp=4,replaced=0;
  while (srcp<srcc) {
    int qkc=src[srcp++];
    if (!qkc) break;
    if (srcp>=srcc) return -1;
    int qvc=src[srcp++];
    if (srcp>srcc-qvc-qkc) return -1;
    if ((qkc==kc)&&!memcmp(src+srcp,k,kc)) {
      if (sr_encode_u8(dst,kc)<0) return -1;
      if (sr_encode_u8(dst,vc)<0) return -1;
      if (sr_encode_raw(dst,k,kc)<0) return -1;
      if (sr_encode_raw(dst,v,vc)<0) return -1;
      replaced=1;
    } else {
      if (sr_encode_u8(dst,qkc)<0) return -1;
      if (sr_encode_u8(dst,qvc)<0) return -1;
      if (sr_encode_raw(dst,src+srcp,qkc+qvc)<0) return -1;
    }
    srcp+=qkc+qvc;
  }
  if (!replaced) {
    if (sr_encode_u8(dst,kc)<0) return -1;
    if (sr_encode_u8(dst,vc)<0) return -1;
    if (sr_encode_raw(dst,k,kc)<0) return -1;
    if (sr_encode_raw(dst,v,vc)<0) return -1;
  }
  if (sr_encode_u8(dst,0)<0) return -1;
  return 0;
}

/* Decode the ROM file and add our icon to it.
 * This means adding the icon image resource and also adding a field to metadata:1.
 */
 
static int pbltool_unbundle_html_repack_rom(struct pbltool_unbundle_html *ctx) {
  int err;
  ctx->scratch.c=0;
  if ((err=pbltool_unbundle_html_decode_base64(&ctx->scratch,ctx->b64,ctx->b64c,ctx->srcpath))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error decoding base64\n",ctx->srcpath);
    return -2;
  }
  if ((err=pbltool_rom_add_archive(&ctx->rom,ctx->scratch.v,ctx->scratch.c,ctx->srcpath))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error decoding ROM\n",ctx->srcpath);
    return -2;
  }
  
  // Choose the lowest available image rid.
  int rid=1;
  struct pbltool_res *res=ctx->rom.resv;
  int i=ctx->rom.resc;
  for (;i-->0;res++) {
    if (res->tid<PBL_TID_image) continue;
    if (res->tid>PBL_TID_image) break;
    if (res->rid>rid) break;
    if (res->rid>=0xffff) { // unlikely!
      fprintf(stderr,"%s: No available image id for app icon\n",ctx->srcpath);
      return -2;
    }
    rid=res->rid+1;
  }
  
  // Decode the image's base64. pbltool_rom_add_archive copies each resource, so scratch is available now.
  ctx->scratch.c=0;
  const char *src=ctx->icon;
  int srcc=ctx->iconc;
  for (i=srcc;i-->0;) {
    if ((src[i]==',')||(src[i]==';')||(src[i]==':')) {
      src=src+i+1;
      srcc=srcc-i-1;
      break;
    }
  }
  if ((err=pbltool_unbundle_html_decode_base64(&ctx->scratch,src,srcc,ctx->srcpath))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error decode base64 for icon image\n",ctx->srcpath);
    return -2;
  }
  
  // Add image resource.
  int p=pbltool_rom_search(&ctx->rom,PBL_TID_image,rid);
  if (p>=0) return -1;
  p=-p-1;
  res=pbltool_rom_add_res(&ctx->rom,p,PBL_TID_image,rid);
  if (!res) return -1;
  if (pbltool_res_set_serial(res,ctx->scratch.v,ctx->scratch.c)<0) return -1;
  
  // Rewrite metadata:1, inserting "iconImage#={{rid}}"
  res=ctx->rom.resv;
  if ((ctx->rom.resc<1)||(res->tid!=PBL_TID_metadata)||(res->rid!=1)) {
    fprintf(stderr,"%s: ROM does not begin with metadata:1 as it is required to\n",ctx->srcpath);
    return -2;
  }
  char tmp[16];
  int tmpc=sr_decsint_repr(tmp,sizeof(tmp),rid);
  if ((tmpc<1)||(tmpc>sizeof(tmp))) return -1;
  ctx->scratch.c=0;
  if ((err=pbltool_unbundle_rewrite_metadata(&ctx->scratch,res->serial,res->serialc,"iconImage#",10,tmp,tmpc))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error rewriting ROM metadata\n",ctx->srcpath);
    return -2;
  }
  if (pbltool_res_set_serial(res,ctx->scratch.v,ctx->scratch.c)<0) return -1;
  
  return 0;
}

/* Extract ROM from HTML.
 */
 
static int pbltool_unbundle_html(struct sr_encoder *dst,const char *src,int srcc,const char *srcpath) {
  struct pbltool_unbundle_html ctx={.srcpath=srcpath};
  int dstc0=dst->c;
  int err=pbltool_unbundle_html_scan(&ctx,src,srcc,srcpath);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error scanning HTML\n",srcpath);
    pbltool_unbundle_html_cleanup(&ctx);
    return -2;
  }
  if (ctx.iconc) {
    if ((err=pbltool_unbundle_html_repack_rom(&ctx))<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error repacking ROM\n",srcpath);
      pbltool_unbundle_html_cleanup(&ctx);
      return -2;
    }
    if ((err=pbltool_rom_encode(dst,&ctx.rom))<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error reencoding ROM\n",srcpath);
      pbltool_unbundle_html_cleanup(&ctx);
      return -2;
    }
  } else {
    // No icon, we can emit the ROM verbatim without decoding it.
    if ((err=pbltool_unbundle_html_decode_base64(dst,ctx.b64,ctx.b64c,srcpath))<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error decoding base64 ROM\n",srcpath);
      pbltool_unbundle_html_cleanup(&ctx);
      return -2;
    }
  }
  int len=dst->c-dstc0;
  if ((len<8)||memcmp(((char*)dst->v)+dstc0,"\x00\xffPBLROM",8)) {
    fprintf(stderr,"%s: Reencoding did not yield a valid ROM\n",srcpath);
    pbltool_unbundle_html_cleanup(&ctx);
    return -2;
  }
  pbltool_unbundle_html_cleanup(&ctx);
  return 0;
}

/* Test whether this buffer contains a coherent ROM file.
 * Return its length if so.
 */
 
static int pbltool_unbundle_test_rom(const uint8_t *src,int srcc) {
  if (srcc<8) return 0;
  if (memcmp(src,"\x00\xffPBLROM",8)) return 0;
  int srcp=8,tid=1,rid=1;
  int terminated=0,resc=0;
  int metadata1=0;
  while (srcp<srcc) {
    uint8_t lead=src[srcp++];
    if (!lead) {
      terminated=1;
      break;
    }
    switch (lead&0xc0) {
      case 0x00: {
          tid+=lead;
          rid=1;
          if (tid>0x3f) return 0;
        } break;
      case 0x40: {
          if (srcp>srcc-1) return 0;
          int d=(lead&0x3f)<<8;
          d|=src[srcp++];
          rid+=d;
          if (rid>0xffff) return 0;
        } break;
      case 0x80: {
          int len=lead&0x3f;
          len+=1;
          if (srcp>srcc-len) return 0;
          if (rid>0xffff) return 0;
          resc++;
          if ((tid==PBL_TID_metadata)&&(rid==1)) metadata1=1;
          srcp+=len;
          rid++;
        } break;
      case 0xc0: {
          if (srcp>srcc-2) return 0;
          int len=(lead&0x3f)<<16;
          len|=src[srcp++]<<8;
          len|=src[srcp++];
          len+=65;
          if (srcp>srcc-len) return 0;
          if (rid>0xffff) return 0;
          resc++;
          if ((tid==PBL_TID_metadata)&&(rid==1)) metadata1=1;
          srcp+=len;
          rid++;
        } break;
    }
  }
  if (!metadata1) return 0;
  // We could check for code:1, but actually true-native executables won't have one.
  // Could fail if any metadadta or code other than 1 present.
  // But I think overall, the framing will catch invalid ROMs.
  return srcp;
}

/* Extract ROM in memory.
 */
 
static int pbltool_unbundle_extract(struct sr_encoder *dst,const void *src,int srcc,const char *srcpath) {
  
  // HTML files should begin with "<!DOCTYPE". Depend on it.
  if ((srcc>=9)&&!memcmp(src,"<!DOCTYPE",9)) return pbltool_unbundle_html(dst,src,srcc,srcpath);
  
  /* We could call `nm` to list the members of the executable, and find pbl_embedded_rom in there.
   * But two problems:
   *  - The executable might be for a different architecture; I would like it still to be unbundlable.
   *  - Output format of `nm` might vary from host to host, and it might not be available at all sometimes.
   * So why not try something sneakier?
   * Find the signature "\x00\xffPBLROM" and start decoding from there, if it looks coherent go with it.
   * Beware that there definitely will be false matches.
   */
  const uint8_t *SRC=src;
  int srcp=0;
  while (srcp<srcc) {
    int romc=pbltool_unbundle_test_rom(SRC+srcp,srcc-srcp);
    if (romc<1) {
      srcp++;
      continue;
    }
    if (sr_encode_raw(dst,SRC+srcp,romc)<0) return -1;
    return 0;
  }
  fprintf(stderr,"%s: Failed to locate ROM in executable\n",srcpath);
  return -2;
}

/* unbundle, main.
 */
 
int pbltool_main_unbundle() {
  if (!pbltool.dstpath) {
    fprintf(stderr,"%s: Please specify output path as '-oPATH'\n",pbltool.exename);
    return -2;
  }
  if (pbltool.srcpathc!=1) {
    fprintf(stderr,"%s: unbundle requires exactly one input file\n",pbltool.exename);
    return -2;
  }
  void *src=0;
  int srcc=file_read(&src,pbltool.srcpathv[0]);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file\n",pbltool.srcpathv[0]);
    return -2;
  }
  struct sr_encoder dst={0};
  int err=pbltool_unbundle_extract(&dst,src,srcc,pbltool.srcpathv[0]);
  free(src);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error extracting ROM\n",pbltool.srcpathv[0]);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  err=file_write(pbltool.dstpath,dst.v,dst.c);
  sr_encoder_cleanup(&dst);
  if (err<0) {
    fprintf(stderr,"%s: Failed to write file, %d bytes\n",pbltool.dstpath,dst.c);
    return -2;
  }
  return 0;
}
