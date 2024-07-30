#include "pbltool_internal.h"

/* This limit could be removed, but then we'd have to be careful about checking for integer overflow at compile.
 * But it serves as a sanity check too: Using crazy high index with gaps in between will cause large runs of zeroes in the output, to skip.
 */
#define PBLTOOL_STRING_INDEX_LIMIT 8192

/* Binary from text.
 */
 
static int pbltool_strings_bin_from_text(struct sr_encoder *dst,const char *src,int srcc,struct pbltool_res *res) {
  if (sr_encode_raw(dst,"\x00PS\xff",4)<0) return -1;
  struct sr_decoder decoder={.v=src,.c=srcc};
  const char *line;
  int linec,lineno=1,expectindex=0;
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { line++; linec--; }
    if (!linec||(line[0]=='#')) continue;
    
    if ((line[0]<'0')||(line[0]>'9')) {
      fprintf(stderr,"%s:%d: Line must begin with index.\n",res->path,lineno);
      return -2;
    }
    int linep=0,index=0;
    while ((linep<linec)&&((unsigned char)line[linep]>0x20)) {
      int digit=line[linep++]-'0';
      if ((digit<0)||(digit>9)) {
        fprintf(stderr,"%s:%d: Malformed index.\n",res->path,lineno);
        return -2;
      }
      index*=10;
      index+=digit;
      if (index>=PBLTOOL_STRING_INDEX_LIMIT) {
        fprintf(stderr,"%s:%d: Please keep index under %d.\n",res->path,lineno,PBLTOOL_STRING_INDEX_LIMIT);
        return -2;
      }
    }
    if (index<expectindex) {
      fprintf(stderr,"%s:%d: Index %d out of place or already used.\n",res->path,lineno,index);
      return -2;
    }
    while (expectindex<index) {
      if (sr_encode_raw(dst,"\0\0",2)<0) return -1;
      expectindex++;
    }
    expectindex=index+1;
    
    int lenp=dst->c;
    if (sr_encode_raw(dst,"\0\0",2)<0) return -1;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    if ((linep<linec)&&(line[linep]=='"')) {
      struct sr_decoder tmp={.v=line+linep,.c=linec-linep};
      if ((sr_decode_json_string_to_encoder(dst,&tmp)<0)||(tmp.p<tmp.c)) {
        fprintf(stderr,"%s:%d: Malformed JSON string token.\n",res->path,lineno);
        return -2;
      }
    } else {
      if (sr_encode_raw(dst,line+linep,linec-linep)<0) return -1;
    }
    int len=dst->c-lenp-2;
    if (len<0) return -1;
    if (len>0xffff) {
      fprintf(stderr,"%s:%d: String too long, limit 65535\n",res->path,lineno);
      return -2;
    }
    uint8_t *lendst=((uint8_t*)dst->v)+lenp;
    *lendst++=len>>8;
    *lendst=len;
  }
  return 0;
}

/* Text from binary.
 */
 
static int pbltool_string_requires_quote(const char *src,int srcc) {
  if (srcc<1) return 0;
  if (src[0]=='"') return 1;
  if ((unsigned char)src[0]<=0x20) return 1;
  if ((unsigned char)src[srcc-1]<=0x20) return 1;
  // Any C0 byte, request quoting.
  // I guess somebody could overencode a newline or something, if they want to mess with us, but whatever.
  for (;srcc-->0;src++) {
    if ((unsigned char)(*src)<0x20) return 1;
  }
  return 0;
}
 
static int pbltool_strings_text_from_bin(struct sr_encoder *dst,const uint8_t *src,int srcc,struct pbltool_res *res) {
  if ((srcc<4)||memcmp(src,"\x00PS\xff",4)) return -1;
  int srcp=4,index=0;
  while (srcp<srcc) {
    if (srcp>srcc-2) {
      fprintf(stderr,"%s: Unexpected end of file.\n",res->path);
      return -2;
    }
    int len=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (srcp>srcc-len) {
      fprintf(stderr,"%s: Unexpected end of file.\n",res->path);
      return -2;
    }
    if (len) {
      if (sr_encode_fmt(dst,"%d ",index)<0) return -1;
      const char *v=(char*)(src+srcp);
      if (pbltool_string_requires_quote(v,len)) {
        if (sr_encode_json_string(dst,0,0,v,len)<0) return -1;
      } else {
        if (sr_encode_raw(dst,v,len)<0) return -1;
      }
      if (sr_encode_u8(dst,0x0a)<0) return -1;
    }
    index++;
    srcp+=len;
  }
  return 0;
}

/* Compile strings, main entry point.
 */
 
int pbltool_compile_strings(struct pbltool_res *res) {
  if ((res->serialc>=4)&&!memcmp(res->serial,"\x00PS\xff",4)) return 0;
  struct sr_encoder dst={0};
  int err=pbltool_strings_bin_from_text(&dst,res->serial,res->serialc,res);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error compiling strings.\n",res->path);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  pbltool_res_handoff_serial(res,dst.v,dst.c);
  return 0;
}

/* Uncompile strings, main entry point.
 */
 
int pbltool_uncompile_strings(struct pbltool_res *res) {
  if ((res->serialc>=4)&&!memcmp(res->serial,"\x00PS\xff",4)) {
    struct sr_encoder dst={0};
    int err=pbltool_strings_text_from_bin(&dst,res->serial,res->serialc,res);
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error uncompiling strings.\n",res->path);
      sr_encoder_cleanup(&dst);
      return -2;
    }
    pbltool_res_handoff_serial(res,dst.v,dst.c);
    return 0;
  }
  return 0;
}
