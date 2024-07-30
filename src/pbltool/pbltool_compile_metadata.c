#include "pbltool_internal.h"

/* Binary from text.
 */
 
static int pbltool_metadata_bin_from_text(struct sr_encoder *dst,const char *src,int srcc,struct pbltool_res *res) {
  if (sr_encode_raw(dst,"\x00PM\xff",4)<0) return -1;
  struct sr_decoder decoder={.v=src,.c=srcc};
  const char *line;
  int linec,lineno=1;
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { line++; linec--; }
    if (!linec||(line[0]=='#')) continue;
    const char *k=line;
    int kc=0,linep=0;
    while ((linep<linec)&&(line[linep]!='=')&&(line[linep]!=':')) { linep++; kc++; }
    while (kc&&((unsigned char)k[kc-1]<=0x20)) kc--;
    if (!kc) {
      fprintf(stderr,"%s:%d: Illegal empty key.\n",res->path,lineno);
      return -2;
    }
    if (kc>0xff) {
      fprintf(stderr,"%s:%d: Key too long (%d, limit 255)\n",res->path,lineno,kc);
      return -2;
    }
    if (linep>=linec) {
      fprintf(stderr,"%s:%d: Expected '=' or ':' after key '%.*s'\n",res->path,lineno,kc,k);
      return -2;
    }
    linep++; // skip separator
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    const char *v=line+linep;
    int vc=linec-linep;
    if (vc>0xff) {
      fprintf(stderr,"%s:%d: Value too long (%d, limit 255)\n",res->path,lineno,vc);
      return -2;
    }
    // We could assert that they're both ASCII, and assert formatting of standard fields.
    // But that's not really our problem. There will be a separate "validate ROM" command for pbltool.
    if (sr_encode_u8(dst,kc)<0) return -1;
    if (sr_encode_u8(dst,vc)<0) return -1;
    if (sr_encode_raw(dst,k,kc)<0) return -1;
    if (sr_encode_raw(dst,v,vc)<0) return -1;
  }
  //TODO Maybe we should allow an option to insert "time" if not specified? And other fields like that?
  if (sr_encode_u8(dst,0)<0) return -1;
  return 0;
}

/* Text from binary.
 */
 
static int pbltool_metadata_text_from_bin(struct sr_encoder *dst,const uint8_t *src,int srcc,struct pbltool_res *res) {
  if ((srcc<4)||memcmp(res->serial,"\x00PM\xff",4)) return -1;
  int srcp=4;
  while (srcp<srcc) {
    uint8_t kc=src[srcp++];
    if (!kc) break;
    uint8_t vc=src[srcp++];
    if (srcp>srcc-vc-kc) {
      fprintf(stderr,"%s: Metadata fields overflow resource around %d/%d\n",res->path,srcp-2,srcc);
      return -2;
    }
    const char *k=(char*)(src+srcp); srcp+=kc;
    const char *v=(char*)(src+srcp); srcp+=vc;
    if (sr_encode_fmt(dst,"%.*s=%.*s\n",kc,k,vc,v)<0) return -1;
  }
  return 0;
}

/* Compile metadata, main entry points.
 */
 
int pbltool_compile_metadata(struct pbltool_res *res) {
  if ((res->serialc>=4)&&!memcmp(res->serial,"\x00PM\xff",4)) return 0;
  struct sr_encoder dst={0};
  int err=pbltool_metadata_bin_from_text(&dst,res->serial,res->serialc,res);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error compiling metadata.\n",res->path);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  pbltool_res_handoff_serial(res,dst.v,dst.c);
  return 0;
}
 
int pbltool_uncompile_metadata(struct pbltool_res *res) {
  if ((res->serialc>=4)&&!memcmp(res->serial,"\x00PM\xff",4)) {
    struct sr_encoder dst={0};
    int err=pbltool_metadata_text_from_bin(&dst,res->serial,res->serialc,res);
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error uncompiling metadata.\n",res->path);
      sr_encoder_cleanup(&dst);
      return -2;
    }
    pbltool_res_handoff_serial(res,dst.v,dst.c);
    return 0;
  }
  return 0;
}
