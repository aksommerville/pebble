#include "pbltool_internal.h"

/* default: TID:RID SIZE
 */
 
static int pbltool_list_default(struct pbltool_rom *rom,const char *path) {
  const struct pbltool_res *res=rom->resv;
  int i=rom->resc;
  for (;i-->0;res++) {
    char tname[256];
    int tnamec=pbltool_tid_repr(tname,sizeof(tname),res->tid);
    if ((tnamec<1)||(tnamec>sizeof(tname))) return -1;
    fprintf(stdout,"%20.*s:%-5d %6d\n",tnamec,tname,res->rid,res->serialc);
  }
  return 0;
}

/* numeric: TID:RID SIZE
 */
 
static int pbltool_list_numeric(struct pbltool_rom *rom,const char *path) {
  const struct pbltool_res *res=rom->resv;
  int i=rom->resc;
  for (;i-->0;res++) {
    fprintf(stdout,"%2d:%-5d %6d\n",res->tid,res->rid,res->serialc);
  }
  return 0;
}

/* names: TID NAME RID, named only
 */
 
static int pbltool_list_names(struct pbltool_rom *rom,const char *path) {
  const struct pbltool_res *res=rom->resv;
  int i=rom->resc;
  for (;i-->0;res++) {
    if (!res->name) continue;
    char tname[256];
    int tnamec=pbltool_tid_repr(tname,sizeof(tname),res->tid);
    if ((tnamec<1)||(tnamec>sizeof(tname))) return -1;
    fprintf(stdout,"%.*s %s %d\n",tnamec,tname,res->name,res->rid);
  }
  return 0;
}

/* json: {tid,rid,size,name?}[]
 */
 
static int pbltool_list_json(struct pbltool_rom *rom,const char *path) {
  char sep='[';
  const struct pbltool_res *res=rom->resv;
  int i=rom->resc;
  for (;i-->0;res++) {
    fprintf(stdout,"%c{\"tid\":%d,\"rid\":%d,\"size\":%d",sep,res->tid,res->rid,res->serialc);
    sep=',';
    if (res->name) {
      fprintf(stdout,",\"name\":\"%s\"",res->name);
    }
    fprintf(stdout,"}");
  }
  if (sep=='[') fprintf(stdout,"[]\n");
  else fprintf(stdout,"]\n");
  return 0;
}

/* summary: TID COUNT TOTAL_SIZE, per type
 */
 
static int pbltool_list_summary(struct pbltool_rom *rom,const char *path) {
  const struct pbltool_res *res=rom->resv;
  int c=rom->resc;
  while (c>0) {
    int subc=1;
    int total=res[0].serialc;
    while ((subc<c)&&(res[subc].tid==res[0].tid)) {
      total+=res[subc].serialc;
      subc++;
    }
    char tname[256];
    int tnamec=pbltool_tid_repr(tname,sizeof(tname),res->tid);
    if ((tnamec<1)||(tnamec>sizeof(tname))) return -1;
    fprintf(stdout,"%20.*s %4d %7d\n",tnamec,tname,subc,total);
    res+=subc;
    c-=subc;
  }
  return 0;
}

/* list, dispatch on format.
 */
 
static int pbltool_list_dispatch(struct pbltool_rom *rom,const char *path,const char *format) {
  if (!format||!format[0]) return pbltool_list_default(rom,path);
  if (!strcmp(format,"default")) return pbltool_list_default(rom,path);
  if (!strcmp(format,"numeric")) return pbltool_list_numeric(rom,path);
  if (!strcmp(format,"names")) return pbltool_list_names(rom,path);
  if (!strcmp(format,"json")) return pbltool_list_json(rom,path);
  if (!strcmp(format,"summary")) return pbltool_list_summary(rom,path);
  fprintf(stderr,"%s: Unknown format '%s'. Expected one of: default numeric names json summary\n",pbltool.exename,format);
  return -2;
}

/* list, main.
 */
 
int pbltool_main_list() {
  if (pbltool.srcpathc!=1) {
    fprintf(stderr,"%s: Require exactly one input file.\n",pbltool.exename);
    return -2;
  }
  struct pbltool_rom rom={0};
  int err=pbltool_rom_add_path(&rom,pbltool.srcpathv[0]);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error reading ROM file.\n",pbltool.srcpathv[0]);
    pbltool_rom_cleanup(&rom);
    return -2;
  }
  err=pbltool_list_dispatch(&rom,pbltool.srcpathv[0],pbltool.format);
  pbltool_rom_cleanup(&rom);
  return err;
}
