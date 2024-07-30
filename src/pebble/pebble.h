/* pebble.h
 */
 
#ifndef PEBBLE_H
#define PEBBLE_H

/* Definitions.
 ********************************************************/

/* Not something you'd need in the game, but it ought to be stated.
 */
#define PBL_FRAMEBUFFER_LIMIT 1024
 
/* Standard resource types.
 * Types defined by the game are formatted similarly, but live in a generated header.
 */
#define PBL_TID_metadata 1
#define PBL_TID_code 2
#define PBL_TID_strings 3
#define PBL_TID_image 4
#define PBL_TID_FOR_EACH \
  _(metadata) \
  _(code) \
  _(strings) \
  _(image)

/* Input device state.
 * CD ("Carrier detect") is set if any device is attached.
 */
#define PBL_BTN_LEFT     0x0001
#define PBL_BTN_RIGHT    0x0002
#define PBL_BTN_UP       0x0004
#define PBL_BTN_DOWN     0x0008
#define PBL_BTN_SOUTH    0x0010
#define PBL_BTN_WEST     0x0020
#define PBL_BTN_EAST     0x0040
#define PBL_BTN_NORTH    0x0080
#define PBL_BTN_L1       0x0100
#define PBL_BTN_R1       0x0200
#define PBL_BTN_L2       0x0400
#define PBL_BTN_R2       0x0800
#define PBL_BTN_AUX1     0x1000
#define PBL_BTN_AUX2     0x2000
#define PBL_BTN_AUX3     0x4000
#define PBL_BTN_CD       0x8000

#define PBL_LANG_FROM_STRING(iso639) ( \
  (((iso639)[0]-'a')<<5)| \
  ((iso639)[1]-'a') \
)

/* Client.
 *********************************************************/

/* Runtime will copy the entire ROM file here before launching.
 * pcm_client_rom_size must be statically initialized with the available size; runtime updates with the actual size.
 * If the buffer is too small, we won't launch.
 */
extern unsigned char pbl_client_rom[];
extern int pbl_client_rom_size;

/* (status) nonzero if we're terminating abnormally.
 */
void pbl_client_quit(int status);

/* The framebuffer size provided here is what we'll expect from pbl_client_render.
 * It should be what you asked for via metadata.
 * If you're not able to produce that size, you must fail.
 * (rate,chanc) are the audio driver's format. If not acceptable, you must fail or disable audio.
 * It is always possible to get (rate,chanc) of zero, eg if the user asked for no audio.
 */
int pbl_client_init(
  int fbw,int fbh,
  int rate,int chanc
);

/* Update model state.
 * (elapsed) in seconds, and will be forced to a sensible interval.
 * (in1..in4) are input state, from 4 possible devices.
 */
void pbl_client_update(double elapsed,int in1,int in2,int in3,int in4);

/* Produce one frame of video.
 * You may return null to preserve the contents of the previous frame (or black initially).
 * Otherwise it must be RGBA, rows packed LRTB, in the size declared to pbl_client_init.
 * All alpha values must be 0xff. I would prefer to ignore them, but for the web host that is a challenge.
 * It doesn't need to be the same pointer every time, and we'll finish reading it before the next update or render.
 * There may be synth calls before we've committed it, if that matters to you.
 * Ordinarily there will be one render after each update. But host is allowed to skip if it feels a need.
 */
void *pbl_client_render();

/* Produce so many samples of audio (not frames, not bytes).
 * We'll copy it out immediately, before any other call to you.
 * You may return null to skip this update and play silence.
 * Beware that nulls may cause some temporal shift of buffers.
 * We don't necessarily emit exactly one update's worth of silence.
 * If you called pbl_set_synth_limit at any point (normally during pbl_client_init),
 * we guarantee not to ask for more than that.
 */
void *pbl_client_synth(int samplec);

/* Host.
 ***********************************************************/

/* Dump some message to the developer's log.
 * This is not typically visible to users (but not secret either).
 * Beware that we are not printf!
 * We automatically append a newline; don't include it.
 * We accept the following constructions:
 *  - %% : literal '%'
 *  - %d : int (decimal, signed)
 *  - %x : int (hexadecimal, unsigned)
 *  - %s : nul-terminated string
 *  - %.*s : int length followed by string.
 *  - %f : double
 *  - %p : pointer
 * We do not accept output field lengths, and other features not listed.
 * Everything we do accept is equivalent to printf.
 */
void pbl_log(const char *fmt,...);

/* The current frame will complete, and host will shut down in an orderly fashion when convenient.
 */
void pbl_terminate(int status);

/* If you call this at any point, we promise not to ask for more than so many synth samples.
 * <=0 is sensible, to disable audio entirely.
 */
void pbl_set_synth_limit(int samplec);

/* Current time in seconds from some undefined epoch.
 * This is real time, not necessarily the sum of (elapsed) to pbl_client_update().
 */
double pbl_now_real();

/* [year,month,day,hour,minute,second,millisecond]
 * Everything is based as you'd expect for presentation.
 */
void pbl_now_local(int *dstv,int dsta);

/* Host selects a language at startup, considering both the user's preference and
 * the preferences you declared in metadata.
 * You can change it to anything at any time.
 * Please note: You should only change it when the user asks you to.
 */
int pbl_get_global_language();
void pbl_set_global_language(int lang);

/* Begin interactive input configuration.
 * If we return >=0, you're going to black out at the end of this frame.
 * (playerid) in 1..4 to remap existing devices, or zero to work with whichever speaks up first.
 * Input config is managed by the host, for the most part you're not privy to it.
 */
int pbl_begin_input_config(int playerid);

/* Persistence.
 * Write an empty string to delete a field.
 * Keys must be 1..255 bytes and values 1..65535.
 * Must be encoded UTF-8.
 */
int pbl_store_get(char *v,int va,const char *k,int kc);
int pbl_store_set(const char *k,int kc,const char *v,int vc);
int pbl_store_key_by_index(char *k,int ka,int p);
 
#endif
