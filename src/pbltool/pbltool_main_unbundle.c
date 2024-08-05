#include "pbltool_internal.h"

/* Extract ROM from HTML.
 */
 
static int pbltool_unbundle_html(struct sr_encoder *dst,const char *src,int srcc,const char *srcpath) {
  //TODO When bundling HTML, we may extract the icon image and turn it into a <link> tag.
  // Is it worth the trouble to reverse that process?
  int srcp=0;
  for (;;srcp++) {
    if (srcp>=srcc-8) {
      fprintf(stderr,"%s: <pbl-rom> tag not found\n",srcpath);
      return -2;
    }
    if (memcmp(src+srcp,"<pbl-rom",8)) continue;
    srcp+=8;
    while ((srcp<srcc)&&(src[srcp++]!='>')) ;
    uint8_t tmp[4];
    int tmpc=0;
    while ((srcp<srcc)&&(src[srcp]!='<')) {
      char ch=src[srcp++];
      if ((unsigned char)ch<=0x20) continue;
           if ((ch>='A')&&(ch<='Z')) tmp[tmpc++]=ch-'A';
      else if ((ch>='a')&&(ch<='z')) tmp[tmpc++]=ch-'a'+26;
      else if ((ch>='0')&&(ch<='9')) tmp[tmpc++]=ch-'0'+52;
      else if (ch=='+') tmp[tmpc++]=62;
      else if (ch=='/') tmp[tmpc++]=63;
      else {
        fprintf(stderr,"%s: Unexpected character '%c' in base64-encoded ROM\n",srcpath,ch);
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
      case 1: {
          if (
            (sr_encode_u8(dst,(tmp[0]<<2))<0)
          ) return -1;
        } break;
      case 2: {
          if (
            (sr_encode_u8(dst,(tmp[0]<<2)|(tmp[1]>>4))<0)||
            (sr_encode_u8(dst,(tmp[1]<<4))<0)
          ) return -1;
        } break;
      case 3: {
          if (
            (sr_encode_u8(dst,(tmp[0]<<2)|(tmp[1]>>4))<0)||
            (sr_encode_u8(dst,(tmp[1]<<4)|(tmp[2]>>2))<0)||
            (sr_encode_u8(dst,(tmp[2]<<6))<0)
          ) return -1;
        } break;
    }
    if ((dst->c<8)||memcmp(dst->v,"\x00\xffPBLROM",8)) {
      fprintf(stderr,"%s: Found and decoded %d bytes of something, but does not have the expected signature.\n",srcpath,dst->c);
      return -2;
    }
    return 0;
  }
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
