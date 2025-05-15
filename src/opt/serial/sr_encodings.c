#include "serial.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

// App programmer! You probably need to tweak these defines to get the right endian.h.
// SHA-1 uses it.
#if USE_macos
  #include <machine/endian.h>
#elif USE_mswin
  #define LITTLE_ENDIAN 1234
  #define BIG_ENDIAN 4321
  #define BYTE_ORDER LITTLE_ENDIAN
#else
  #include <endian.h>
#endif

/* Base64 encode.
 */
 
int sr_base64_encode(char *dst,int dsta,const void *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if ((srcc<0)||(srcc&&!src)) return -1;

  // We emit without any extra formatting, so it's easy to determine the final output length in advance.
  // Do that, and get out quick if the buffer is short.
  int unitc=(srcc+2)/3;
  if (unitc>INT_MAX/4) return -1;
  int dstc=unitc*4;
  if (dstc>dsta) return dstc;

  // Emit the full units. This is either (unitc) or (unitc-1).
  const char *alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const uint8_t *SRC=src;
  int fullunitc=unitc;
  if (srcc%3) fullunitc--;
  for (;fullunitc-->0;SRC+=3,dst+=4) {
    dst[0]=alphabet[SRC[0]>>2];
    dst[1]=alphabet[((SRC[0]<<4)|(SRC[1]>>4))&0x3f];
    dst[2]=alphabet[((SRC[1]<<2)|(SRC[2]>>6))&0x3f];
    dst[3]=alphabet[SRC[2]&0x3f];
  }

  // Emit the partial unit if there is one.
  switch (srcc%3) {
    case 0: break;
    case 1: {
        dst[0]=alphabet[SRC[0]>>2];
        dst[1]=alphabet[(SRC[0]<<4)&0x3f];
        dst[2]='=';
        dst[3]='=';
        dst+=4;
      } break;
    case 2: {
        dst[0]=alphabet[SRC[0]>>2];
        dst[1]=alphabet[((SRC[0]<<4)|(SRC[1]>>4))&0x3f];
        dst[2]=alphabet[(SRC[1]<<2)&0x3f];
        dst[3]='=';
        dst+=4;
      } break;
  }

  if (dstc<dsta) *dst=0;
  return dstc;
}

/* Base64 decode.
 */
 
int sr_base64_decode(void *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  uint8_t *DST=dst;
  int dstc=0,srcp=0;
  uint8_t tmp[4]; // inputs 0..63
  int tmpc=0;
  int finale=0; // how many '=' at the end
  while (srcp<srcc) {
    char indigit=src[srcp++];
    if ((unsigned char)indigit<=0x20) continue;
    if (indigit=='=') { finale++; continue; }
    if (finale) return -1;
    
    if ((indigit>='A')&&(indigit<='Z')) tmp[tmpc++]=indigit-'A';
    else if ((indigit>='a')&&(indigit<='z')) tmp[tmpc++]=indigit-'a'+26;
    else if ((indigit>='0')&&(indigit<='9')) tmp[tmpc++]=indigit-'0'+52;
    else if (indigit=='+') tmp[tmpc++]=62;
    else if (indigit=='/') tmp[tmpc++]=63;
    else return -1;

    if (tmpc>=4) {
      if (dstc<=dsta-3) {
        DST[dstc++]=(tmp[0]<<2)|(tmp[1]>>4);
        DST[dstc++]=(tmp[1]<<4)|(tmp[2]>>2);
        DST[dstc++]=(tmp[2]<<6)|tmp[3];
      } else dstc+=3;
      tmpc=0;
    }
  }
  // We could assert that (finale) takes us to the end of a unit, but I'm not sure the spec actually requires it, and also I don't care.
  switch (tmpc) {
    case 0: break; // Ended on a code unit, nice.
    case 1: { // tmpc==1 should not happen, but if so, assume the 2 missing low bits are zero.
        if (dstc<dsta) DST[dstc]=tmp[0]<<2;
        dstc++;
      } break;
    case 2: { // "AA==" => 1 byte
        if (dstc<dsta) DST[dstc]=(tmp[0]<<2)|(tmp[1]>>4);
        dstc++;
      } break;
    case 3: { // "AAA=" => 2 bytes
        if (dstc<=dsta-2) {
          DST[dstc++]=(tmp[0]<<2)|(tmp[1]>>4);
          DST[dstc++]=(tmp[1]<<4)|(tmp[2]>>2);
        } else dstc+=2;
      } break;
  }
  if (dstc<dsta) DST[dstc]=0;
  return dstc;
}

/* URL encode.
 */

int sr_url_encode(char *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int dstc=0,srcp=0;
  for (;srcp<srcc;srcp++) {
    if (
      ((src[srcp]>='a')&&(src[srcp]<='z'))||
      ((src[srcp]>='A')&&(src[srcp]<='Z'))||
      ((src[srcp]>='0')&&(src[srcp]<='9'))||
      (src[srcp]=='!')||
      (src[srcp]=='\'')||
      (src[srcp]=='(')||
      (src[srcp]==')')||
      (src[srcp]=='*')||
      (src[srcp]=='-')||
      (src[srcp]=='.')||
      (src[srcp]=='_')||
      (src[srcp]=='~')||
    0) {
      if (dstc<dsta) dst[dstc]=src[srcp];
      dstc++;
    } else {
      if (dstc<=dsta-3) {
        dst[dstc++]='%';
        dst[dstc++]="0123456789ABCDEF"[(src[srcp]>>4)&15];
        dst[dstc++]="0123456789ABCDEF"[src[srcp]&15];
      } else dstc+=3;
    }
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* URL decode.
 */
 
int sr_url_decode(char *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int dstc=0,srcp=0;
  while (srcp<srcc) {
    if (src[srcp]=='%') {
      if (srcp>srcc-3) return -1;
      int hi=sr_digit_eval(src[srcp+1]);
      int lo=sr_digit_eval(src[srcp+2]);
      if ((hi<0)||(hi>15)||(lo<0)||(lo>15)) return -1;
      if (dstc<dsta) dst[dstc]=(hi<<4)|lo;
      dstc++;
      srcp+=3;
    } else {
      if (dstc<dsta) dst[dstc]=src[srcp];
      dstc++;
      srcp++;
    }
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/*
 * RFC 1321 compliant MD5 implementation,
 * by Christophe Devine <devine@cr0.net>;
 * this program is licensed under the GPL.
 */

struct md5_context {
  uint32_t total[2];
  uint32_t state[4];
  uint8_t buffer[64];
};

#define GET_UINT32(n,b,i)                       \
{                                               \
    (n) = ( (uint32_t) (b)[(i) + 3] << 24 )       \
        | ( (uint32_t) (b)[(i) + 2] << 16 )       \
        | ( (uint32_t) (b)[(i) + 1] <<  8 )       \
        | ( (uint32_t) (b)[(i)    ]       );      \
}

#define PUT_UINT32(n,b,i)                       \
{                                               \
    (b)[(i)    ] = (uint8_t) ( (n)       );       \
    (b)[(i) + 1] = (uint8_t) ( (n) >>  8 );       \
    (b)[(i) + 2] = (uint8_t) ( (n) >> 16 );       \
    (b)[(i) + 3] = (uint8_t) ( (n) >> 24 );       \
}

static void md5_starts( struct md5_context *ctx )
{
    ctx->total[0] = 0;
    ctx->total[1] = 0;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
}

static void md5_process( struct md5_context *ctx, const uint8_t *data/*64*/ )
{
    uint32_t A, B, C, D, X[16];

    GET_UINT32( X[0],  data,  0 );
    GET_UINT32( X[1],  data,  4 );
    GET_UINT32( X[2],  data,  8 );
    GET_UINT32( X[3],  data, 12 );
    GET_UINT32( X[4],  data, 16 );
    GET_UINT32( X[5],  data, 20 );
    GET_UINT32( X[6],  data, 24 );
    GET_UINT32( X[7],  data, 28 );
    GET_UINT32( X[8],  data, 32 );
    GET_UINT32( X[9],  data, 36 );
    GET_UINT32( X[10], data, 40 );
    GET_UINT32( X[11], data, 44 );
    GET_UINT32( X[12], data, 48 );
    GET_UINT32( X[13], data, 52 );
    GET_UINT32( X[14], data, 56 );
    GET_UINT32( X[15], data, 60 );

#define S(x,n) ((x << n) | ((x & 0xFFFFFFFF) >> (32 - n)))

#define P(a,b,c,d,k,s,t)                                \
{                                                       \
    a += F(b,c,d) + X[k] + t; a = S(a,s) + b;           \
}

    A = ctx->state[0];
    B = ctx->state[1];
    C = ctx->state[2];
    D = ctx->state[3];

#define F(x,y,z) (z ^ (x & (y ^ z)))

    P( A, B, C, D,  0,  7, 0xD76AA478 );
    P( D, A, B, C,  1, 12, 0xE8C7B756 );
    P( C, D, A, B,  2, 17, 0x242070DB );
    P( B, C, D, A,  3, 22, 0xC1BDCEEE );
    P( A, B, C, D,  4,  7, 0xF57C0FAF );
    P( D, A, B, C,  5, 12, 0x4787C62A );
    P( C, D, A, B,  6, 17, 0xA8304613 );
    P( B, C, D, A,  7, 22, 0xFD469501 );
    P( A, B, C, D,  8,  7, 0x698098D8 );
    P( D, A, B, C,  9, 12, 0x8B44F7AF );
    P( C, D, A, B, 10, 17, 0xFFFF5BB1 );
    P( B, C, D, A, 11, 22, 0x895CD7BE );
    P( A, B, C, D, 12,  7, 0x6B901122 );
    P( D, A, B, C, 13, 12, 0xFD987193 );
    P( C, D, A, B, 14, 17, 0xA679438E );
    P( B, C, D, A, 15, 22, 0x49B40821 );

#undef F

#define F(x,y,z) (y ^ (z & (x ^ y)))

    P( A, B, C, D,  1,  5, 0xF61E2562 );
    P( D, A, B, C,  6,  9, 0xC040B340 );
    P( C, D, A, B, 11, 14, 0x265E5A51 );
    P( B, C, D, A,  0, 20, 0xE9B6C7AA );
    P( A, B, C, D,  5,  5, 0xD62F105D );
    P( D, A, B, C, 10,  9, 0x02441453 );
    P( C, D, A, B, 15, 14, 0xD8A1E681 );
    P( B, C, D, A,  4, 20, 0xE7D3FBC8 );
    P( A, B, C, D,  9,  5, 0x21E1CDE6 );
    P( D, A, B, C, 14,  9, 0xC33707D6 );
    P( C, D, A, B,  3, 14, 0xF4D50D87 );
    P( B, C, D, A,  8, 20, 0x455A14ED );
    P( A, B, C, D, 13,  5, 0xA9E3E905 );
    P( D, A, B, C,  2,  9, 0xFCEFA3F8 );
    P( C, D, A, B,  7, 14, 0x676F02D9 );
    P( B, C, D, A, 12, 20, 0x8D2A4C8A );

#undef F
   
#define F(x,y,z) (x ^ y ^ z)

    P( A, B, C, D,  5,  4, 0xFFFA3942 );
    P( D, A, B, C,  8, 11, 0x8771F681 );
    P( C, D, A, B, 11, 16, 0x6D9D6122 );
    P( B, C, D, A, 14, 23, 0xFDE5380C );
    P( A, B, C, D,  1,  4, 0xA4BEEA44 );
    P( D, A, B, C,  4, 11, 0x4BDECFA9 );
    P( C, D, A, B,  7, 16, 0xF6BB4B60 );
    P( B, C, D, A, 10, 23, 0xBEBFBC70 );
    P( A, B, C, D, 13,  4, 0x289B7EC6 );
    P( D, A, B, C,  0, 11, 0xEAA127FA );
    P( C, D, A, B,  3, 16, 0xD4EF3085 );
    P( B, C, D, A,  6, 23, 0x04881D05 );
    P( A, B, C, D,  9,  4, 0xD9D4D039 );
    P( D, A, B, C, 12, 11, 0xE6DB99E5 );
    P( C, D, A, B, 15, 16, 0x1FA27CF8 );
    P( B, C, D, A,  2, 23, 0xC4AC5665 );

#undef F

#define F(x,y,z) (y ^ (x | ~z))

    P( A, B, C, D,  0,  6, 0xF4292244 );
    P( D, A, B, C,  7, 10, 0x432AFF97 );
    P( C, D, A, B, 14, 15, 0xAB9423A7 );
    P( B, C, D, A,  5, 21, 0xFC93A039 );
    P( A, B, C, D, 12,  6, 0x655B59C3 );
    P( D, A, B, C,  3, 10, 0x8F0CCC92 );
    P( C, D, A, B, 10, 15, 0xFFEFF47D );
    P( B, C, D, A,  1, 21, 0x85845DD1 );
    P( A, B, C, D,  8,  6, 0x6FA87E4F );
    P( D, A, B, C, 15, 10, 0xFE2CE6E0 );
    P( C, D, A, B,  6, 15, 0xA3014314 );
    P( B, C, D, A, 13, 21, 0x4E0811A1 );
    P( A, B, C, D,  4,  6, 0xF7537E82 );
    P( D, A, B, C, 11, 10, 0xBD3AF235 );
    P( C, D, A, B,  2, 15, 0x2AD7D2BB );
    P( B, C, D, A,  9, 21, 0xEB86D391 );

#undef F

    ctx->state[0] += A;
    ctx->state[1] += B;
    ctx->state[2] += C;
    ctx->state[3] += D;
}

static void md5_update( struct md5_context *ctx, const uint8_t *input, uint32_t length )
{
    uint32_t left, fill;

    if ( ! length ) return;

    left = ( ctx->total[0] >> 3 ) & 0x3F;
    fill = 64 - left;

    ctx->total[0] += length <<  3;
    ctx->total[1] += length >> 29;

    ctx->total[0] &= 0xFFFFFFFF;
    ctx->total[1] += ctx->total[0] < ( length << 3 );

    if ( left && length >= fill )
    {
        memcpy( (void *) (ctx->buffer + left), (void *) input, fill );
        md5_process( ctx, ctx->buffer );
        length -= fill;
        input  += fill;
        left = 0;
    }

    while ( length >= 64 )
    {
        md5_process( ctx, input );
        length -= 64;
        input  += 64;
    }

    if ( length )
    {
        memcpy( (void *) (ctx->buffer + left), (void *) input, length );
    }
}

static uint8_t md5_padding[64] =
{
 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static void md5_finish( struct md5_context *ctx, uint8_t digest[16] )
{
    uint32_t last, padn;
    uint8_t msglen[8];

    PUT_UINT32( ctx->total[0], msglen, 0 );
    PUT_UINT32( ctx->total[1], msglen, 4 );

    last = ( ctx->total[0] >> 3 ) & 0x3F;
    padn = ( last < 56 ) ? ( 56 - last ) : ( 120 - last );

    md5_update( ctx, md5_padding, padn );
    md5_update( ctx, msglen, 8 );

    PUT_UINT32( ctx->state[0], digest,  0 );
    PUT_UINT32( ctx->state[1], digest,  4 );
    PUT_UINT32( ctx->state[2], digest,  8 );
    PUT_UINT32( ctx->state[3], digest, 12 );
}

int sr_md5(void *dst,int dsta,const void *src,int srcc) {
  if (!dst||(dsta<16)) return 16;
  if ((srcc<0)||(srcc&&!src)) return -1;
  struct md5_context ctx;
  md5_starts(&ctx);
  md5_update(&ctx,src,srcc);
  md5_finish(&ctx,dst);
  return 16;
}

