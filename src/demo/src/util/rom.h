/* rom.h
 * Helpers for extracting data from your ROM.
 *
 * To acquire the ROM image initially:
 *   int romlen=pbl_rom_get(0,0);
 *   void *rom=malloc(romlen);
 *   if (!rom) ...error...
 *   pbl_rom_get(rom,romlen);
 */
 
#ifndef ROM_H
#define ROM_H

/* Convenient form with global state, libc, and pebble api calls.
 * This is probably the section you want.
 * But if you're building bare-bones, can remove this and rom_stateful.c.
 * We store a global TOC of resources for fast search.
 * We do not cache metadata or strings content; those get looked up from scratch each time.
 ***********************************************************************/

/* Drop all global state.
 * No need to do this at termination, unless you want to.
 * In fact, I'm not sure why this should exist at all. :P
 */
void rom_quit();

/* Fetches the rom, then decodes and collates it.
 * Quietly noops if already initialized.
 */
int rom_init();

/* Inform us of a language change.
 * We'll get it from Pebble initially, but you have to notify if it changes after.
 * We DO NOT change Pebble's language on rom_set_language(), we assume you've done that.
 * This influences rom_get_metadata() and rom_get_string(().
 */
void rom_set_language(int lang);
int rom_get_language();

/* Get one resource.
 * Zero if we can't find it, never negative.
 * We'll put a pointer into our rom at (*dstpp). Do not free or modify.
 */
int rom_get(void *dstpp,int tid,int rid);

/* Get one field from metadata:1, resolving strings if appropriate.
 * Strings-resolvable keys are named like "myKey@"=(default) and "myKey$"=(index in strings:1).
 * If you want string resolution, ask for "myKey" without the suffix.
 * Asking for "myKey@" or "myKey$" with the suffix will return the exact stored value under those keys.
 * Do not free or modify the returned string.
 */
int rom_get_metadata(void *dstpp,const char *k,int kc);

/* Look up one string in a strings resource, resolving against the global language.
 * (subrid) in 1..63.
 * Do not free or modify the returned string.
 */
int rom_get_string(void *dstpp,int subrid,int index);

/* Stateless primitives.
 * If you're doing a minimal build, might keep just these and rom_stateless.c.
 * libc is not required (nor anything else).
 *************************************************************************/

/* Calls (cb) for each resource, in order.
 * If the ROM is invalid, returns -1.
 * That can happen after some resources have been reported already.
 * Return nonzero from (cb) to stop iteration and return the same.
 */
int rom_for_each(
  const void *rom,int romc,
  int (*cb)(int tid,int rid,const void *serial,int serialc,void *userdata),
  void *userdata
);

/* Find one resource in the ROM by ID.
 * Note that this has to read the whole ROM from the start.
 * If you're getting multiple resources, it's usually better to use rom_for_each().
 * Never returns negative; empty*, missing, and error are indistinguishable.
 * [*] There's no such thing as empty.
 */
int rom_seek(void *dstpp,const void *rom,int romc,int tid,int rid);

/* Calls (cb) for each field in a given metadata resource.
 * Return nonzero to stop iteration and return the same.
 * -1 for malformed resource.
 */
int rom_metadata_for_each(
  const void *meta,int metac,
  int (*cb)(const char *k,int kc,const char *v,int vc,void *userdata),
  void *userdata
);

/* Calls (cb) for each non-empty string in a strings resource.
 * Empty strings are encoded just like full ones, we skip them as a convenience.
 * (index) from zero. There may be gaps but it will always increase.
 * Return nonzero to stop iteration and return the same.
 * -1 for malformed resource.
 */
int rom_strings_for_each(
  const void *strings,int stringsc,
  int (*cb)(int index,const char *v,int c,void *userdata),
  void *userdata
);

/* Fetch just one string from a strings resource.
 * Equivalent behavior can be made with rom_strings_for_each(),
 * but this is both more convenient and more efficient, for a single lookup.
 * Zero for errors, never negative.
 */
int rom_strings_get_by_index(void *dstpp,const void *strings,int stringsc,int index);

#endif
