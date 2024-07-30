#include "pbltool_html.h"
#include "opt/serial/serial.h"
#include <string.h>
#include <stdio.h>

/* Text helpers.
 */
 
int pbltool_html_count_newlines(const char *src,int srcc) {
  int nlc=0;
  for (;srcc-->0;src++) {
    if (*src==0x0a) nlc++;
  }
  return nlc;
}

/* Init.
 */
 
void pbltool_html_decoder_init(struct pbltool_html_decoder *decoder,const char *src,int srcc,const char *path) {
  memset(decoder,0,sizeof(struct pbltool_html_decoder));
  decoder->src=src;
  decoder->srcc=srcc;
  decoder->path=path;
  decoder->lineno=1;
}

/* Next chunk.
 */

int pbltool_html_decoder_next(void *dstpp,struct pbltool_html_decoder *decoder) {

  // Consume the previous chunk, and return zero at EOF.
  decoder->lineno+=pbltool_html_count_newlines(decoder->src+decoder->srcp,decoder->nextc);
  decoder->srcp+=decoder->nextc;
  decoder->nextc=0;
  decoder->nodetype=0;
  if (decoder->srcp>=decoder->srcc) return 0;
  const char *src=decoder->src+decoder->srcp;
  int srcc=decoder->srcc-decoder->srcp;
  
  /* Most things begin with a less-than: COMMENT DOCTYPE SINGLETON OPEN CLOSE
   */
  if (src[0]=='<') {
    if (srcc<2) return -1;
    if (src[1]=='/') {
      decoder->nodetype=PBLTOOL_HTML_NODETYPE_CLOSE;
      decoder->nextc=2;
      while ((decoder->nextc<srcc)&&(src[decoder->nextc]!='>')) decoder->nextc++;
      if (decoder->nextc>=srcc) return -1;
      decoder->nextc++;
      *(const void**)dstpp=src;
      return decoder->nextc;
    }
    if (src[1]=='!') {
      if ((srcc>=4)&&!memcmp(src,"<!--",4)) {
        decoder->nodetype=PBLTOOL_HTML_NODETYPE_COMMENT;
        decoder->nextc=4;
        for (;;) {
          if (decoder->nextc>srcc-3) return -1;
          if (!memcmp(src+decoder->nextc,"-->",3)) {
            decoder->nextc+=3;
            break;
          }
          decoder->nextc++;
        }
        *(const void**)dstpp=src;
        return decoder->nextc;
      } else if ((srcc>=9)&&!memcmp(src,"<!DOCTYPE",9)) {
        decoder->nodetype=PBLTOOL_HTML_NODETYPE_DOCTYPE;
        decoder->nextc=9;
        for (;;) {
          if (decoder->nextc>=srcc) return -1;
          if (src[decoder->nextc++]=='>') break;
        }
        *(const void**)dstpp=src;
        return decoder->nextc;
      } else {
        return -1;
      }
    }
    // SINGLETON if the last non-space char before '>' is '/'; otherwise OPEN.
    decoder->nodetype=PBLTOOL_HTML_NODETYPE_OPEN;
    decoder->nextc=1;
    for (;;) {
      if (decoder->nextc>=srcc) return -1;
      if ((unsigned char)src[decoder->nextc]<=0x20) {
        decoder->nextc++;
        continue;
      }
      if (src[decoder->nextc]=='>') {
        decoder->nextc++;
        break;
      }
      if (src[decoder->nextc]=='/') {
        decoder->nodetype=PBLTOOL_HTML_NODETYPE_SINGLETON;
      } else {
        decoder->nodetype=PBLTOOL_HTML_NODETYPE_OPEN;
      }
      decoder->nextc++;
    }
    *(const void**)dstpp=src;
    return decoder->nextc;
  }
  
  // TEXT or SPACE until the next '<'. It's SPACE until we discover otherwise.
  // Touching EOF here is fine.
  decoder->nodetype=PBLTOOL_HTML_NODETYPE_SPACE;
  decoder->nextc=0;
  while (decoder->nextc<srcc) {
    if (src[decoder->nextc]=='<') break;
    if ((unsigned char)src[decoder->nextc]>0x20) decoder->nodetype=PBLTOOL_HTML_NODETYPE_TEXT;
    decoder->nextc++;
  }
  *(const void**)dstpp=src;
  return decoder->nextc;
}

/* Skip body and tail of an OPEN tag.
 */
 
int pbltool_html_decoder_skip_tag(void *dstpp,struct pbltool_html_decoder *decoder) {
  if (decoder->nodetype!=PBLTOOL_HTML_NODETYPE_OPEN) return -1;
  int lineno0=decoder->lineno;
  int startp=decoder->srcp+decoder->nextc;
  if (dstpp) *(const void**)dstpp=decoder->src+startp;
  const char *mainname=0;
  int mainnamec=pbltool_html_tag_name(&mainname,decoder->src+decoder->srcp,decoder->nextc);
  #define DEPTH_LIMIT 16
  struct tagname {
    const char *v;
    int c;
    int lineno;
  } tagnamev[DEPTH_LIMIT];
  int tagnamec=0;
  for (;;) {
    const char *node=0;
    int nodec=pbltool_html_decoder_next(&node,decoder);
    if (nodec<0) return nodec;
    if (!nodec) {
      fprintf(stderr,"%s:%d: Unclosed <%.*s> tag\n",decoder->path,lineno0,mainnamec,mainname);
      return -2;
    }
    
    if (decoder->nodetype==PBLTOOL_HTML_NODETYPE_OPEN) {
      const char *name=0;
      int namec=pbltool_html_tag_name(&name,node,nodec);
      if (tagnamec>=DEPTH_LIMIT) {
        fprintf(stderr,"%s:%d: Depth limit exceeded at <%.*s> tag\n",decoder->path,decoder->lineno,namec,name);
        return -2;
      }
      struct tagname *tag=tagnamev+tagnamec++;
      tag->v=name;
      tag->c=namec;
      tag->lineno=decoder->lineno;
      continue;
    }
    
    if (decoder->nodetype==PBLTOOL_HTML_NODETYPE_CLOSE) {
      const char *name=0;
      int namec=pbltool_html_tag_name(&name,node,nodec);
      if (tagnamec) {
        tagnamec--;
        const struct tagname *tag=tagnamev+tagnamec;
        if ((namec!=tag->c)||memcmp(name,tag->v,namec)) {
          fprintf(stderr,"%s:%d: Expected </%.*s> for tag on line %d, found </%.*s> instead.\n",decoder->path,decoder->lineno,tag->c,tag->v,tag->lineno,namec,name);
          return -2;
        }
      } else if ((namec==mainnamec)&&!memcmp(name,mainname,namec)) {
        return decoder->srcp-startp;
      } else {
        fprintf(stderr,"%s:%d: Expected </%.*s> for tag on line %d, found </%.*s> instead.\n",decoder->path,decoder->lineno,mainnamec,mainname,lineno0,namec,name);
        return -2;
      }
    }
  }
  #undef DEPTH_LIMIT
}

/* Pick data out of HTML tags.
 */

int pbltool_html_tag_name(void *dstpp,const char *src,int srcc) {
  if ((srcc<1)||(src[0]!='<')) return 0;
  int srcp=1;
  while ((srcp<srcc)&&(((unsigned char)src[srcp]<=0x20)||(src[srcp]=='/'))) srcp++;
  *(const void**)dstpp=src+srcp;
  int namec=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) break;
    if (src[srcp]=='=') break;
    if (src[srcp]=='/') break;
    if (src[srcp]=='>') break;
    srcp++;
    namec++;
  }
  return namec;
}

int pbltool_html_attribute(void *dstpp,const char *src,int srcc,const char *k,int kc) {
  if ((srcc<1)||(src[0]!='<')) return 0;
  int srcp=1;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    if (src[srcp]=='/') { srcp++; continue; }
    if (src[srcp]=='>') break;
    const char *qk=src+srcp,*v=0;
    int qkc=0,vc=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)&&(src[srcp]!='=')&&(src[srcp]!='/')) { srcp++; qkc++; }
    if ((srcp<srcc)&&(src[srcp]=='=')) {
      srcp++;
      if ((srcp<srcc)&&(src[srcp]=='"')) {
        srcp++;
        v=src+srcp;
        while ((srcp<srcc)&&(src[srcp]!='"')) { srcp++; vc++; }
        if ((srcp<srcc)&&(src[srcp]=='"')) srcp++;
      } else {
        v=src+srcp;
        while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)&&(src[srcp]!='/')&&(src[srcp]!='>')) { srcp++; vc++; }
      }
    } else {
      v=qk;
      vc=qkc;
    }
    if ((qkc==kc)&&!memcmp(qk,k,kc)) {
      if (dstpp) *(const void**)dstpp=v;
      return vc;
    }
  }
  return 0;
}

/* Emit text with escapes.
 */
 
int pbltool_html_encode_text(struct sr_encoder *dst,const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0;
  while (srcp<srcc) {
    if (src[srcp]=='<') {
      if (sr_encode_raw(dst,"&lt;",4)<0) return -1;
      srcp++;
    } else if (src[srcp]=='&') {
      if (sr_encode_raw(dst,"&amp;",5)<0) return -1;
      srcp++;
    } else {
      int rawc=1;
      while ((srcp+rawc<srcc)&&(src[srcp+rawc]!='<')&&(src[srcp+rawc]!='&')) rawc++;
      if (sr_encode_raw(dst,src+srcp,rawc)<0) return -1;
      srcp+=rawc;
    }
  }
  return 0;
}

/* Javascript space and comments.
 */
 
static int pbltool_js_measure_space(const char *src,int srcc) {
  int srcp=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) {
      srcp++;
      continue;
    }
    if (srcp>srcc-2) return srcp;
    if (src[srcp]!='/') return srcp;
    switch (src[srcp+1]) {
      case '/': {
          srcp+=2;
          while ((srcp<srcc)&&(src[srcp++]!=0x0a)) ;
        } continue;
      case '*': {
          srcp+=2;
          for (;;) {
            if (srcp>srcc-2) return -1;
            if ((src[srcp]=='*')&&(src[srcp+1]=='/')) { srcp+=2; break; }
            srcp++;
          }
        } continue;
    }
    return srcp;
  }
  return srcp;
}

/* Measure Javascript.
 */
 
int pbltool_js_measure(const char *src,int srcc) {
  if (!src||(srcc<1)) return 0;
  
  if ((unsigned char)src[0]<=0x20) return pbltool_js_measure_space(src,srcc);
  if ((srcc>=2)&&(src[0]=='/')) {
    if (src[1]=='/') return pbltool_js_measure_space(src,srcc);
    if (src[1]=='*') return pbltool_js_measure_space(src,srcc);
  }
  
  // Special regex detection.
  if ((srcc>=2)&&(src[0]=='(')&&(src[1]=='/')) {
    int srcp=2;
    for (;;) {
      if (srcp>=srcc) return -1;
      if (src[srcp]=='\\') srcp+=2;
      else if (src[srcp++]=='/') return srcp;
    }
  }
  
  // Strings.
  if ((src[0]=='"')||(src[0]=='\'')||(src[0]=='`')) {
    int srcp=1;
    for (;;) {
      if (srcp>=srcc) return -1;
      if (src[srcp]=='\\') srcp+=2;
      else if (src[srcp++]==src[0]) return srcp;
    }
  }
  
  // "import" statements: Consume the whole line.
  if ((srcc>=7)&&!memcmp(src,"import",6)&&((unsigned char)src[6]<=0x20)) {
    int srcp=7;
    while ((srcp<srcc)&&(src[srcp]!=0x0a)) srcp++;
    return srcp;
  }
  
  // Everything else, read until space, slash, open-paren, or quote.
  int srcp=1;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) break;
    if (src[srcp]=='/') break;
    if (src[srcp]=='(') break;
    if (src[srcp]=='"') break;
    if (src[srcp]=='\'') break;
    if (src[srcp]=='`') break;
    srcp++;
  }
  return srcp;
}
