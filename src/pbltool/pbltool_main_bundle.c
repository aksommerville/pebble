#include "pbltool_internal.h"
#include "pbltool_html.h"

/* Determine path to HTML template.
 */
 
static int pbltool_bundle_get_html_template_path(char *dst,int dsta) {
  if (pbltool.template&&pbltool.template[0]) {
    const char *src=pbltool.template;
    int srcc=0; while (src[srcc]) srcc++;
    if (srcc<=dsta) {
      memcpy(dst,src,srcc);
      if (srcc<dsta) dst[srcc]=0;
    }
    return srcc;
  }
  int dstc=snprintf(dst,dsta,"%s/src/web/index.html",PBL_SDK);
  if ((dstc<1)||(dstc>=dsta)) return -1;
  return dstc;
}

/* Compose a local path from something in the source, and its referrer.
 */
 
static int pbltool_resolve_path(char *dst,int dsta,const char *referrer,int referrerc,const char *relpath,int relpathc) {
  if (!relpath) return -1;
  if (relpathc<0) { relpathc=0; while (relpath[relpathc]) relpathc++; }
  if (!relpathc) return -1;
  
  // Absolute path means relative to the original template.
  if (relpath[0]=='/') {
    char tmpath[1024];
    int tmpathc=pbltool_bundle_get_html_template_path(tmpath,sizeof(tmpath));
    if ((tmpathc<1)||(tmpathc>=sizeof(tmpath))) return -1;
    relpath++;
    relpathc--;
    return pbltool_resolve_path(dst,dsta,tmpath,tmpathc,relpath,relpathc);
  }
  
  // Drop basename from referrer.
  if (!referrer) referrerc=0; else if (referrerc<0) { referrerc=0; while (referrer[referrerc]) referrerc++; }
  while (referrerc&&(referrer[referrerc-1]!='/')) referrerc--;
  
  // Drop empty and "." from head of (relpath), and at ".." back referrer up by one directory.
  while (relpathc>0) {
    if (relpath[0]=='/') {
      relpath++;
      relpathc--;
      continue;
    }
    int leadc=1;
    while ((leadc<relpathc)&&(relpath[leadc]!='/')) leadc++;
    if ((leadc==1)&&(relpath[0]=='.')) {
      relpath++;
      relpathc--;
      continue;
    }
    if ((leadc==2)&&(relpath[0]=='.')&&(relpath[1]=='.')) {
      relpath+=2;
      relpathc-=2;
      while (referrerc&&(referrer[referrerc-1]=='/')) referrerc--;
      while (referrerc&&(referrer[referrerc-1]!='/')) referrerc--;
      continue;
    }
    break;
  }
  
  // If there remain any empty, dot, or double-dot components, fail.
  // These would interfere with path matching.
  int p=0;
  while (p<relpathc) {
    const char *comp=relpath+p;
    int compc=0;
    while ((p+compc<relpathc)&&(relpath[p++]!='/')) compc++;
    if (!compc||((compc==1)&&(comp[0]=='.'))||((compc==2)&&(comp[0]=='.')&&(comp[1]=='.'))) {
      return -1;
    }
  }
  
  int dstc=snprintf(dst,dsta,"%.*s%.*s",referrerc,referrer,relpathc,relpath);
  if ((dstc<1)||(dstc>=dsta)) return -1;
  return dstc;
}

/* Javascript include list.
 * This is global, but we're the only consumer of it.
 */
 
void pbltool_jsinclude_clear() {
  while (pbltool.jsincludec>0) {
    pbltool.jsincludec--;
    free(pbltool.jsincludev[pbltool.jsincludec]);
  }
}

int pbltool_jsinclude_intern(const char *path,int pathc) {
  if (!path) return -1;
  if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  if (!pathc) return -1;
  int i=pbltool.jsincludec;
  while (i-->0) {
    if (memcmp(path,pbltool.jsincludev[i],pathc)) continue;
    if (pbltool.jsincludev[i][pathc]) continue;
    return 0;
  }
  if (pbltool.jsincludec>=pbltool.jsincludea) {
    int na=pbltool.jsincludea+16;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(pbltool.jsincludev,sizeof(void*)*na);
    if (!nv) return -1;
    pbltool.jsincludev=nv;
    pbltool.jsincludea=na;
  }
  char *nv=malloc(pathc+1);
  if (!nv) return -1;
  memcpy(nv,path,pathc);
  nv[pathc]=0;
  pbltool.jsincludev[pbltool.jsincludec++]=nv;
  return 1;
}

/* Append encoded ROM file, with HTML fences.
 */
 
static int pbltool_encode_base64(struct sr_encoder *dst,const uint8_t *src,int srcc) {
  const char *alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int stopp=srcc-srcc%3;
  int srcp=0;
  int nlclock=30;
  for (;srcp<stopp;srcp+=3) {
    uint8_t tmp[4];
    tmp[0]=alphabet[src[srcp]>>2];
    tmp[1]=alphabet[((src[srcp]<<4)|(src[srcp+1]>>4))&0x3f];
    tmp[2]=alphabet[((src[srcp+1]<<2)|(src[srcp+2]>>6))&0x3f];
    tmp[3]=alphabet[src[srcp+2]&0x3f];
    if (sr_encode_raw(dst,tmp,4)<0) return -1;
    if (nlclock--<0) {
      if (sr_encode_u8(dst,0x0a)<0) return -1;
      nlclock=30;
    }
  }
  switch (srcc-srcp) {
    case 1: {
        uint8_t tmp[2];
        tmp[0]=alphabet[src[srcp]>>2];
        tmp[1]=alphabet[(src[srcp]<<4)&0x3f];
        if (sr_encode_raw(dst,tmp,2)<0) return -1;
      } break;
    case 2: {
        uint8_t tmp[3];
        tmp[0]=alphabet[src[srcp]>>2];
        tmp[1]=alphabet[((src[srcp]<<4)|(src[srcp+1]>>4))&0x3f];
        tmp[2]=alphabet[(src[srcp+1]<<2)&0x3f];
        if (sr_encode_raw(dst,tmp,3)<0) return -1;
      } break;
  }
  return 0;
}
 
static int pbltool_inline_rom(struct sr_encoder *dst,struct pbltool_rom *rom) {
  if (sr_encode_raw(dst,"<pbl-rom style=\"display:none\">\n",-1)<0) return -1;
  int err=pbltool_rom_validate(rom);
  if (err<0) return err;
  struct sr_encoder tmp={0};
  if ((err=pbltool_rom_encode(&tmp,rom))<0) {
    sr_encoder_cleanup(&tmp);
    return err;
  }
  err=pbltool_encode_base64(dst,tmp.v,tmp.c);
  sr_encoder_cleanup(&tmp);
  if (err<0) return err;
  if (sr_encode_raw(dst,"</pbl-rom>",-1)<0) return -1;
  return 0;
}

/* Javascript identifier test.
 */
 
static int pbltool_js_isident(char ch) {
  if ((ch>='a')&&(ch<='z')) return 1;
  if ((ch>='A')&&(ch<='Z')) return 1;
  if ((ch>='0')&&(ch<='9')) return 1;
  if (ch=='_') return 1;
  if (ch=='$') return 1;
  return 0;
}

/* Minify one Javascript file.
 * I say "minify", but what we do is pretty weak.
 */
 
static int pbltool_inline_javascript(struct sr_encoder *dst,const char *referrer,const char *relpath,int relpathc);
 
static int pbltool_inline_javascript_inner(struct sr_encoder *dst,const char *src,int srcc,const char *path) {
  int srcp=0,lineno=1;
  while (srcp<srcc) {
    const char *token=src+srcp;
    int tokenc=pbltool_js_measure(src+srcp,srcc-srcp);
    if (tokenc<1) {
      fprintf(stderr,"%s:%d: Javascript tokenization error.\n",path,lineno);
      return -2;
    }
    int tlineno=lineno;
    srcp+=tokenc;
    lineno+=pbltool_html_count_newlines(token,tokenc);
    
    // Drop whitespace, comments, and the word "export".
    if ((unsigned char)token[0]<=0x20) continue;
    if ((tokenc>=2)&&(token[0]=='/')&&(token[1]=='/')) continue;
    if ((tokenc>=2)&&(token[0]=='/')&&(token[1]=='*')) continue;
    if ((tokenc==6)&&!memcmp(token,"export",6)) continue;
    
    // Inline imports the moment we find them.
    // We're way out of compliance here: All we look at is the path.
    if ((tokenc>=7)&&!memcmp(token,"import",6)&&((unsigned char)token[6]<=0x20)) {
      int tp=7;
      while ((tp<tokenc)&&(token[tp]!='"')&&(token[tp]!='\'')) tp++;
      if (tp>=tokenc) {
        fprintf(stderr,"%s:%d: Malformed import statement.\n",path,tlineno);
        return -2;
      }
      char quote=token[tp++];
      const char *relpath=token+tp;
      int relpathc=0;
      while ((tp<tokenc)&&(token[tp++]!=quote)) relpathc++;
      int err=pbltool_inline_javascript(dst,path,relpath,relpathc);
      if (err<0) {
        if (err!=-2) fprintf(stderr,"%s: Unspecified error importing '%.*s'\n",path,relpathc,relpath);
        return -2;
      }
      continue;
    }
    
    // If the last character emitted and first of the new token are both identifier-legal, emit a space.
    if (dst->c&&pbltool_js_isident(((char*)dst->v)[dst->c-1])&&pbltool_js_isident(token[0])) {
      if (sr_encode_u8(dst,0x20)<0) return -1;
    }
    
    // Finally, emit the text verbatim.
    if (sr_encode_raw(dst,token,tokenc)<0) return -1;
  }
  if (sr_encode_u8(dst,0x0a)<0) return -1;
  return 0;
}

/* Locate a Javascript file, read it, minify it, and append to encoder.
 * Output always ends with a newline if not empty.
 */
 
static int pbltool_inline_javascript(struct sr_encoder *dst,const char *referrer,const char *relpath,int relpathc) {
  char path[1024];
  int pathc=pbltool_resolve_path(path,sizeof(path),referrer,-1,relpath,relpathc);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  int err=pbltool_jsinclude_intern(path,pathc);
  if (err<=0) return err; // error or already had it.
  char *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read Javascript file.\n",path);
    return -2;
  }
  err=pbltool_inline_javascript_inner(dst,src,srcc,path);
  free(src);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error inlining Javascript\n",path);
    return -2;
  }
  return 0;
}

/* Locate a CSS file, read it, minify it, and append to encoder.
 * Output always ends with a newline if not empty.
 * We're applying only a light touch to CSS. It has some funny rules around white space.
 */
 
static int pbltool_inline_css_inner(struct sr_encoder *dst,const char *src,int srcc,const char *path) {
  int srcp=0,lineno=1;
  while (srcp<srcc) {
    
    // Drop comments.
    if ((src[srcp]=='/')&&(srcp<srcc-1)&&(src[srcp+1]=='*')) {
      srcp+=2;
      int oline=lineno;
      for (;;) {
        if (srcp>srcc-2) {
          fprintf(stderr,"%s:%d: Unclosed comment.\n",path,oline);
          return -2;
        }
        if ((src[srcp]=='*')&&(src[srcp+1]=='/')) {
          srcp+=2;
          break;
        }
        if (src[srcp]==0x0a) lineno++;
        srcp++;
      }
      continue;
    }
    
    // Reduce runs of whitespace to a single space or newline.
    // It leaves plenty of unnecessary space in place. But some spaces are significant and I don't trust myself to distinguish them.
    if ((unsigned char)src[srcp]<=0x20) {
      while (srcp<srcc) {
        if (src[srcp]==0x0a) lineno++;
        if ((unsigned char)src[srcp]>0x20) break;
        srcp++;
      }
      // Newline if the last character was '}', otherwise space.
      if ((dst->c>0)&&(((char*)dst->v)[dst->c-1]=='}')) {
        if (sr_encode_u8(dst,0x0a)<0) return -1;
      } else {
        if (sr_encode_u8(dst,0x20)<0) return -1;
      }
      continue;
    }
    
    // Verbatim to the next slash or whitespace.
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)&&(src[srcp]!='/')) { srcp++; tokenc++; }
    if (sr_encode_raw(dst,token,tokenc)<0) return -1;
  }
  if (dst->c&&(((char*)dst->v)[dst->c-1]!=0x0a)) {
    if (sr_encode_u8(dst,0x0a)<0) return -1;
  }
  return 0;
}
 
static int pbltool_inline_css(struct sr_encoder *dst,const char *referrer,const char *relpath,int relpathc,struct pbltool_rom *rom) {
  char path[1024];
  int pathc=pbltool_resolve_path(path,sizeof(path),referrer,-1,relpath,relpathc);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  char *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read CSS file.\n",path);
    return -2;
  }
  int err=pbltool_inline_css_inner(dst,src,srcc,path);
  free(src);
  return err;
}

/* Inline the favicon.
 * The rules are complex:
 *  - If [href] is missing or empty, remove the embedded icon from rom and inline it.
 *  - If [href] is a data URL, preserve it exactly.
 *  - Anything else, it must be an image file resolvable from here. Inline it.
 * If there's no <link rel="icon"> tag, we won't create one. (and that's not a decision we could make from here).
 */
 
static int pbltool_inline_favicon(
  struct sr_encoder *dst,
  const char *referrer,
  const char *relpath,int relpathc,
  const char *tag,int tagc,
  struct pbltool_rom *rom
) {
  if (!relpathc) {
    const char *imageidstr=0;
    int imageidstrc=pbltool_rom_get_meta(&imageidstr,rom,"iconImage#",10);
    int imageid=0;
    if ((sr_int_eval(&imageid,imageidstr,imageidstrc)>=2)&&(imageid>0)) {
      const void *serial=0;
      int serialc=pbltool_rom_get(&serial,rom,PBL_TID_image,imageid);
      if (serialc>0) {
        const char *mimetype=pbltool_guess_mime_type(serial,serialc,"",0);
        if (sr_encode_fmt(dst,"\n<link rel=\"icon\" href=\"data:%s;base64,",mimetype)<0) return -1;
        int estlen=(serialc*4+3)/3;
        if (sr_encoder_require(dst,estlen)<0) return -1;
        int addc=sr_base64_encode((char*)dst->v+dst->c,dst->a-dst->c,serial,serialc);
        if ((addc<0)||(dst->c>dst->a-addc)) return -1;
        dst->c+=addc;
        if (sr_encode_raw(dst,"\"/>\n",-1)<0) return -1;
        pbltool_rom_clear_resource(rom,PBL_TID_image,imageid);
        pbltool_rom_remove_meta(rom,"iconImage#",10);
        return 0;
      }
    }
    return 0;
  }
  if ((relpathc>=5)&&!memcmp(relpath,"data:",5)) {
    if (sr_encode_raw(dst,tag,tagc)<0) return -1;
    return 0;
  }
  char iconpath[1024];
  int iconpathc=pbltool_resolve_path(iconpath,sizeof(iconpath),referrer,-1,relpath,relpathc);
  if ((iconpathc<1)||(iconpathc>=sizeof(iconpath))) return -1;
  void *serial=0;
  int serialc=file_read(&serial,iconpath);
  if (serialc<0) {
    fprintf(stderr,"%s: Failed to read file for favicon.\n",iconpath);
    return -2;
  }
  const char *mimetype=pbltool_guess_mime_type(serial,serialc,"",0);
  if (sr_encode_fmt(dst,"\n<link rel=\"icon\" href=\"data:%s;base64,",mimetype)<0) { free(serial); return -1; }
  int estlen=(serialc*4+3)/3;
  if (sr_encoder_require(dst,estlen)<0) { free(serial); return -1; }
  int addc=sr_base64_encode((char*)dst->v+dst->c,dst->a-dst->c,serial,serialc);
  if ((addc<0)||(dst->c>dst->a-addc)) { free(serial); return -1; }
  dst->c+=addc;
  if (sr_encode_raw(dst,"\"/>\n",-1)<0) { free(serial); return -1; }
  free(serial);
  return 0;
}

/* Append a <title> tag to (dst).
 */
 
static int pbltool_insert_title(struct sr_encoder *dst,const char *prev,int prevc,struct pbltool_rom *rom) {
  while (prevc&&((unsigned char)prev[prevc-1]<=0x20)) prevc--;
  while (prevc&&((unsigned char)prev[0]<=0x20)) { prev++; prevc--; }
  const char *fromrom=0;
  int fromromc=pbltool_rom_get_meta(&fromrom,rom,"title@",6);
  if (fromromc<1) {
    fromrom=prev;
    fromromc=prevc;
  }
  if (fromromc>0) {
    if (sr_encode_raw(dst,"<title>",7)<0) return -1;
    if (pbltool_html_encode_text(dst,fromrom,fromromc)<0) return -1;
    if (sr_encode_raw(dst,"</title>",8)<0) return -1;
  }
  return 0;
}

/* bundle HTML, in context.
 */
 
static int pbltool_bundle_html_inner(struct sr_encoder *dst,struct pbltool_html_decoder *decoder,struct pbltool_rom *rom) {
  #define DEPTH_LIMIT 16
  struct tagname {
    const char *v;
    int c;
    int lineno;
  } tagnamev[DEPTH_LIMIT];
  int tagnamec=0;
  int emitted_rom=0;
  int err=0;
  for (;;) {
    const char *node=0;
    int nodec=pbltool_html_decoder_next(&node,decoder);
    if (!nodec) break;
    if (nodec<0) {
      if (nodec!=-2) fprintf(stderr,"%s:%d: Unspecified error parsing HTML.\n",decoder->path,decoder->lineno);
      return -2;
    }
    switch (decoder->nodetype) {
      case PBLTOOL_HTML_NODETYPE_TEXT: { // Trim space and emit verbatim.
          while (nodec&&((unsigned char)node[nodec-1]<=0x20)) nodec--;
          while (nodec&&((unsigned char)node[0]<=0x20)) { nodec--; node++; }
          if (sr_encode_raw(dst,node,nodec)<0) return -1;
        } break;
      case PBLTOOL_HTML_NODETYPE_SPACE: break; // Ignore.
      case PBLTOOL_HTML_NODETYPE_COMMENT: break; // Ignore.
      case PBLTOOL_HTML_NODETYPE_DOCTYPE: { // Emit verbatim, plus a newline.
          if (sr_encode_raw(dst,node,nodec)<0) return -1;
          if (sr_encode_u8(dst,0x0a)<0) return -1;
        } break;
        
      case PBLTOOL_HTML_NODETYPE_SINGLETON: { // Inline or emit verbatim.
          const char *name=0;
          int namec=pbltool_html_tag_name(&name,node,nodec);
          if ((namec==4)&&!memcmp(name,"link",4)) {
            const char *rel=0,*href=0;
            int relc=pbltool_html_attribute(&rel,node,nodec,"rel",3);
            int hrefc=pbltool_html_attribute(&href,node,nodec,"href",4);
            if ((relc==10)&&!memcmp(rel,"stylesheet",10)) {
              if (sr_encode_raw(dst,"<style>\n",-1)<0) return -1;
              if ((err=pbltool_inline_css(dst,decoder->path,href,hrefc,rom))<0) {
                if (err!=-2) fprintf(stderr,"%s:%d: Unspecified error inlining CSS.\n",decoder->path,decoder->lineno);
                return -2;
              }
              if (sr_encode_raw(dst,"</style>",-1)<0) return -1;
              continue;
            }
            if ((relc==4)&&!memcmp(rel,"icon",4)) {
              if ((err=pbltool_inline_favicon(dst,decoder->path,href,hrefc,node,nodec,rom))<0) {
                if (err!=-2) fprintf(stderr,"%s:%d: Unspecified error inlining favicon.\n",decoder->path,decoder->lineno);
                return -2;
              }
              continue;
            }
          }
          if (sr_encode_raw(dst,node,nodec)<0) return -1;
        } break;
        
      case PBLTOOL_HTML_NODETYPE_OPEN: { // Inline thru close tag, or emit verbatim.
          const char *name=0;
          int namec=pbltool_html_tag_name(&name,node,nodec);
          if (tagnamec>=DEPTH_LIMIT) {
            fprintf(stderr,"%s:%d: Tag depth limit %d exceeded at <%.*s>.\n",decoder->path,decoder->lineno,DEPTH_LIMIT,namec,name);
            return -2;
          }
          if ((namec==5)&&!memcmp(name,"title",5)) {
            const char *prev=0;
            int prevc=pbltool_html_decoder_skip_tag(&prev,decoder);
            if (prevc<0) return prevc;
            if ((err=pbltool_insert_title(dst,prev,prevc,rom))<0) return err;
            continue;
          }
          if ((namec==6)&&!memcmp(name,"script",6)) {
            const char *src=0;
            int srcc=pbltool_html_attribute(&src,node,nodec,"src",3);
            if (sr_encode_raw(dst,"<script>\n",-1)<0) return -1;
            if ((err=pbltool_inline_javascript(dst,decoder->path,src,srcc))<0) {
              if (err!=-2) fprintf(stderr,"%s:%d: Unspecified error inlining Javascript.\n",decoder->path,decoder->lineno);
              return -2;
            }
            if (sr_encode_raw(dst,"</script>",-1)<0) return -1;
            if ((err=pbltool_html_decoder_skip_tag(0,decoder))<0) return err;
            continue;
          }
          struct tagname *tag=tagnamev+tagnamec++;
          tag->v=name;
          tag->c=namec;
          tag->lineno=decoder->lineno;
          if (sr_encode_raw(dst,node,nodec)<0) return -1;
        } break;
        
      case PBLTOOL_HTML_NODETYPE_CLOSE: { // Validate tag stack and emit verbatim.
          const char *name=0;
          int namec=pbltool_html_tag_name(&name,node,nodec);
          if (tagnamec<1) {
            fprintf(stderr,"%s:%d: Unexpected closing tag '%.*s'\n",decoder->path,decoder->lineno,nodec,node);
            return -2;
          }
          tagnamec--;
          const struct tagname *tag=tagnamev+tagnamec;
          if ((tag->c!=namec)||memcmp(tag->v,name,namec)) {
            fprintf(stderr,"%s:%d: Expected closing tag for <%.*s> from line %d, found </%.*s> instead.\n",decoder->path,decoder->lineno,tag->c,tag->v,tag->lineno,namec,name);
            return -2;
          }
          if (!emitted_rom&&(namec==4)&&!memcmp(name,"head",4)) {
            // Insert the ROM just before </head>.
            if ((err=pbltool_inline_rom(dst,rom))<0) {
              if (err!=-2) fprintf(stderr,"%s: Unspecified error inlining ROM file.\n",decoder->path);
              return -2;
            }
            emitted_rom=1;
          }
          if (sr_encode_raw(dst,node,nodec)<0) return -1;
        } break;
        
      default: {
          fprintf(stderr,"%s:%d: Unexpected node type %d. [%s:%d]\n",decoder->path,decoder->lineno,decoder->nodetype,__FILE__,__LINE__);
          return -2;
        }
    }
  }
  
  if (sr_encode_u8(dst,0x0a)<0) return -1;
  if (tagnamec) {
    const struct tagname *tag=tagnamev+tagnamec-1;
    fprintf(stderr,"%s:%d: Unclosed <%.*s>\n",decoder->path,tag->lineno,tag->c,tag->v);
    return -2;
  }
  if (!emitted_rom) {
    fprintf(stderr,"%s: '</head>' was not found. We need that as a marker for inserting the ROM file.\n",decoder->path);
    return -2;
  }
  #undef DEPTH_LIMIT
  return 0;
}

/* bundle HTML.
 */
 
static int pbltool_bundle_html(struct sr_encoder *dst,struct pbltool_rom *rom,const char *rompath) {
  char tmpath[1024];
  int tmpathc=pbltool_bundle_get_html_template_path(tmpath,sizeof(tmpath));
  if ((tmpathc<1)||(tmpathc>=sizeof(tmpath))) {
    fprintf(stderr,"%s: Failed to determine HTML template path.\n",pbltool.exename);
    return -2;
  }
  char *html=0;
  int htmlc=file_read(&html,tmpath);
  if (htmlc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",tmpath);
    return -2;
  }
  struct pbltool_html_decoder decoder;
  pbltool_html_decoder_init(&decoder,html,htmlc,tmpath);
  int err=pbltool_bundle_html_inner(dst,&decoder,rom);
  free(html);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error processing HTML.\n",tmpath);
    return -2;
  }
  return 0;
}

/* bundle true-native with a library of the game's code.
 */
 
static int pbltool_bundle_true_lib(struct sr_encoder *dst,struct pbltool_rom *rom,const char *rompath,const char *libpath) {
  fprintf(stderr,"%s TODO %s\n",__func__,rompath);
  return -2;
}

/* bundle true-native by recompiling code:1 to the host architecture.
 * This is a very interesting case, I'm not sure how possible it is.
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
