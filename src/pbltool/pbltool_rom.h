/* pbltool_rom.h
 * Model of a ROM file designed for mutability.
 */
 
#ifndef PBLTOOL_ROM_H
#define PBLTOOL_ROM_H

struct pbltool_rom {

  struct pbltool_res {
    int tid,rid;
    char *path,*name,*comment,*format;
    int pathc,namec,commentc,formatc;
    void *serial;
    int serialc;
    int seq; // For tracking order of addition.
  } *resv;
  int resc,resa;
  
  // Increment (seq) after adding resources, if you're going to add more.
  // Resources added at a given (seq) must have unique IDs; if (seq) is different, new ones replace old.
  int seq;
};

void pbltool_rom_cleanup(struct pbltool_rom *rom);

/* Directory, resource file, or ROM file, we'll figure it out.
 * ID conflicts within one pbltool_rom_add_path() are reported as errors.
 * ID conflicts across calls, the later one quietly wins.
 */
int pbltool_rom_add_path(struct pbltool_rom *rom,const char *path);
int pbltool_rom_add_archive(struct pbltool_rom *rom,const uint8_t *src,int srcc,const char *path);

/* The resource list stays sorted at all times, and we don't allow conflict.
 */
int pbltool_rom_search(const struct pbltool_rom *rom,int tid,int rid);

/* Populate tmp->(tid,rid,path,name,comment,format) with pointers into (path).
 * This is only a temporary storage space before you create the real res record.
 * Resolving does not guarantee the content is valid.
 */
int pbltool_rom_resolve_ids(struct pbltool_res *tmp,struct pbltool_rom *rom,const char *path,int pathc);

/* Manually add resources.
 * You must determine the IDs beforehand.
 */
struct pbltool_res *pbltool_rom_add_res(struct pbltool_rom *rom,int p,int tid,int rid);
int pbltool_res_set_path(struct pbltool_res *res,const char *src,int srcc);
int pbltool_res_set_name(struct pbltool_res *res,const char *src,int srcc);
int pbltool_res_set_comment(struct pbltool_res *res,const char *src,int srcc);
int pbltool_res_set_format(struct pbltool_res *res,const char *src,int srcc);
int pbltool_res_set_serial(struct pbltool_res *res,const void *src,int srcc);
void pbltool_res_handoff_serial(struct pbltool_res *res,void *src,int srcc);

/* Helpers to read standard features from (comment) and (format).
 */
int pbltool_res_is_verbatim(const struct pbltool_res *res);

/* Generate the canonical basename.
 */
int pbltool_res_compose_base(char *dst,int dsta,const struct pbltool_res *res);

/* Removes empty resources, and if anything looks wrong, log it and fail.
 */
int pbltool_rom_validate(struct pbltool_rom *rom);

/* Produce a ROM file from this model.
 * Errors will be detected and reported but not logged or corrected.
 * It's wise to call pbltool_rom_validate first.
 */
int pbltool_rom_encode(struct sr_encoder *dst,const struct pbltool_rom *rom);

/* Conveniences for reading resources.
 * Reading from meta here does not do any string lookups. (note that we're not aware of the language).
 */
int pbltool_rom_get(void *dstpp,const struct pbltool_rom *rom,int tid,int rid);
int pbltool_rom_get_meta(void *dstpp,const struct pbltool_rom *rom,const char *k,int kc);
int pbltool_rom_get_string(void *dstpp,const struct pbltool_rom *rom,int rid,int index); // Include language in (rid).
void pbltool_rom_clear_resource(struct pbltool_rom *rom,int tid,int rid);
int pbltool_rom_remove_meta(struct pbltool_rom *rom,const char *k,int kc); // Only if compiled.

#endif
