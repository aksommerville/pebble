/* (src) is ArrayBuffer or Uint8Array.
 * Returns string.
 */
export function encodeBase64(src) {
  if (src instanceof ArrayBuffer) src = new Uint8Array(src);
  if (!(src instanceof Uint8Array)) return "";
  let dst = "";
  const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const fullc = src.length - src.length % 3;
  let srcp = 0;
  while (srcp<src.length) {
    const b0 = src[srcp++] || 0;
    const b1 = src[srcp++] || 0;
    const b2 = src[srcp++] || 0;
    dst += alphabet[b0 >> 2];
    dst += alphabet[((b0 << 4) | (b1 >> 4)) & 0x3f];
    dst += alphabet[((b1 << 2) | (b2 >> 6)) & 0x3f];
    dst += alphabet[b2 & 0x3f];
  }
  switch (src.length - srcp) {
    case 1: {
        const b0 = src[srcp++] || 0;
        dst += alphabet[b0 >> 2];
        dst += alphabet[(b0 << 4) & 0x3f];
        dst += "==";
      } break;
    case 2: {
        const b0 = src[srcp++] || 0;
        const b1 = src[srcp++] || 0;
        dst += alphabet[b0 >> 2];
        dst += alphabet[((b0 << 4) | (b1 >> 4)) & 0x3f];
        dst += alphabet[(b1 << 2) & 0x3f];
        dst += "=";
      } break;
  }
  return dst;
}
