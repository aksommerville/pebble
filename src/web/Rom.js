/* Rom.js
 * Manages access to the ROM file.
 */
 
export class Rom {
  constructor(serial) {
    this.serial = serial; // ArrayBuffer
    this.u8 = new Uint8Array(this.serial);
    this.resv = []; // {tid,rid,p,c}, in the order stored (ie some useful constraints).
    this.meta = {}; // [key]:value just as stored.
    this.decode();
  }
  
  /* Resolve string references if applicable.
   * (name) must not contain the trailing "@" or "$".
   * (lang) is numeric, 10 bits.
   */
  getMetaString(name, lang) {
    const stringid = this.meta[name + "$"];
    if (stringid) {
      const string = this.getString((lang << 6) | 1, +stringid);
      if (string) return string;
    }
    return this.meta[name + "@"] || "";
  }
  
  getString(rid, index) {
    if ((index < 0) || isNaN(index)) return "";
    const res = this.getResourceHeader(Rom.TID_strings, rid);
    if (!res) return "";
    return this.stringFromResource(this.u8, res.p, res.c, index);
  }
  
  getResourceHeader(tid, rid) {
    let lo=0, hi=this.resv.length;
    while (lo < hi) {
      const ck = (lo + hi) >> 1;
      const q = this.resv[ck];
           if (tid < q.tid) hi = ck;
      else if (tid > q.tid) lo = ck + 1;
      else if (rid < q.rid) hi = ck;
      else if (rid > q.rid) lo = ck + 1;
      else return q;
    }
    return null;
  }
  
  // Uint8Array or null.
  getResource(tid, rid) {
    const res = this.getResourceHeader(tid, rid);
    if (!res) return null;
    return new Uint8Array(this.serial, res.p, res.c);
  }
  
  getPersistenceIdentifier() {
    let sfx = this.meta.persistKey;
    if (!sfx) sfx = this.meta.version;
    if (!this.meta["title@"] || !sfx) {
      throw new Error(`Persistence not permitted without a title and version or persistKey`);
    }
    return this.meta["title@"] + ":" + sfx;
  }
  
  // Empty string on any error.
  stringFromResource(src, p, c, index) {
    if ((c < 4) || (src[p] !== 0x00) || (src[p + 1] !== 0x50) || (src[p + 2] !== 0x53) || (src[p + 3] !== 0xff)) return "";
    const zp = p + c;
    p += 4;
    let qi = 0;
    while (p < zp) {
      let len = src[p++] << 8;
      len |= src[p++];
      if (len > zp - p) return "";
      if (qi++ === index) {
        try {
          return new TextDecoder("utf8").decode(src.slice(p, p + len));
        } catch (e) {
          return "";
        }
      }
      p += len;
    }
    return "";
  }
  
  /* Assert signature, populate (this.resv), validate metadata:1 and code:1.
   * Since we want to validate its structure anyway, we'll go ahead and decode metadata:1 all the way.
   */
  decode() {
    if (this.u8.length < 8) throw new Error(`Invalid ROM length ${this.u8.length}`);
    if (
      (this.u8[0] !== 0x00) ||
      (this.u8[1] !== 0xff) ||
      (this.u8[2] !== 0x50) || // "PBLROM"
      (this.u8[3] !== 0x42) ||
      (this.u8[4] !== 0x4c) ||
      (this.u8[5] !== 0x52) ||
      (this.u8[6] !== 0x4f) ||
      (this.u8[7] !== 0x4d)
    ) throw new Error(`ROM signature mismatch`);
    this.resv = [];
    for (let srcp=8, tid=1, rid=1; srcp<this.u8.length; ) {
      const lead = this.u8[srcp++];
      if (!lead) break;
      switch (lead&0xc0) {
        case 0x00: {
            tid += lead;
            rid = 1;
          } break;
        case 0x40: {
            if (srcp > this.u8.length - 1) throw new Error(`Unexpected EOF`);
            let d = (lead & 0x3f) << 8;
            d |= this.u8[srcp++];
            rid += d;
          } break;
        case 0x80: {
            let c = lead & 0x3f;
            c += 1;
            if (srcp > this.u8.length - c) throw new Error(`Unexpected EOF`);
            if ((tid > 0x3f) || (rid > 0xffff)) throw new Error(`Invalid TOC`);
            this.resv.push({ tid, rid, p: srcp, c });
            srcp += c;
            rid++;
          } break;
        case 0xc0: {
            if (srcp > this.u8.length - 2) throw new Error(`Unexpected EOF`);
            let c = (lead & 0x3f) << 16;
            c |= this.u8[srcp++] << 8;
            c |= this.u8[srcp++];
            c += 65;
            if (srcp > this.u8.length - c) throw new Error(`Unexpected EOF`);
            if ((tid > 0x3f) || (rid > 0xffff)) throw new Error(`Invalid TOC`);
            this.resv.push({ tid, rid, p: srcp, c });
            srcp += c;
            rid++;
          } break;
      }
    }
    if (
      (this.resv.length < 2) ||
      (this.resv[0].tid !== Rom.TID_metadata) ||
      (this.resv[0].rid !== 1) ||
      (this.resv[1].tid !== Rom.TID_code) ||
      (this.resv[1].rid !== 1)
    ) throw new Error(`Missing metadata:1 or code:1, or unexpected extra metadatas`);
    this.meta = this.decodeMetadata(this.u8, this.resv[0].p, this.resv[0].c);
  }
  
  decodeMetadata(src, p, c) {
    if ((c < 5) || (src[p] !== 0x00) || (src[p + 1] !== 0x50) || (src[p + 2] !== 0x4d) || (src[p + 3] !== 0xff)) {
      throw new Error(`Metadata signature mismatch`);
    }
    const pz = p + c;
    p += 4;
    const meta = {};
    const textDecoder = new TextDecoder("ascii");
    for (;;) {
      if (p >= pz) throw new Error(`Invalid metadata`);
      const kc = src[p++];
      if (!kc) break;
      if (p >= pz) throw new Error(`Invalid metadata`);
      const vc = src[p++];
      if (p > pz - vc - kc) throw new Error(`Invalid metadata`);
      const k = textDecoder.decode(src.slice(p, p + kc));
      p += kc;
      const v = textDecoder.decode(src.slice(p, p + vc));
      p += vc;
      meta[k] = v;
    }
    return meta;
  }
}

Rom.TID_metadata = 1;
Rom.TID_code = 2;
Rom.TID_strings = 3;
Rom.TID_image = 4;
