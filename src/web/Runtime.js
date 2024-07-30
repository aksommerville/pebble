/* Runtime.js
 * Top level of Pebble's web runtime.
 * You supply an HTML element and ArrayBuffer containing the ROM.
 * We will clear the element's content and take it over.
 */
 
import { Rom } from "./Rom.js";
import { Audio } from "./Audio.js";
import { Input } from "./Input.js";
import * as util from "./util.js";
 
export class Runtime {
  constructor(element, serial) {
    this.element = element;
    this.serial = serial; // ArrayBuffer
    this.terminate = false;
    this.termStatus = 0;
    this.synthLimit = 1024;
    this.instance = null;
    this.memory = null; // Uint8Array
    this.textEncoder = new TextEncoder("utf8");
    this.textDecoder = new TextDecoder("utf8");
    this.rom = new Rom(serial);
    this.lang = this.selectDefaultLanguage();
    this.store = this.loadStore();
    this.instate = [0, 0, 0, 0];
    this.prevTimeS = 0;
    this.initAudio();
    this.initInput();
    this.buildUi();
    this.changeTitlePerRom();
    this.changeFaviconPerRom();
  }
  
  /* Stuff that would be part of ctor but needs to be async.
   */
  start() {
    return this.loadWasm();
  }
  
  loadWasm() {
    const serial = this.rom.getResource(Rom.TID_code, 1);
    const imports = {
      env: {
        pbl_log: (fmt, vargs) => this.pbl_log(fmt, vargs),
        pbl_terminate: (status) => { this.terminate = true; this.termStatus = status; },
        pbl_set_synth_limit: (samplec) => { this.synthLimit = samplec; },
        pbl_now_real: () => Date.now() / 1000,
        pbl_now_local: (p, a) => this.pbl_now_local(p, a),
        pbl_get_global_language: () => this.lang,
        pbl_set_global_language: (l) => { this.lang = l; this.changeTitlePerRom(); },
        pbl_begin_input_config: (playerid) => this.pbl_begin_input_config(playerid),
        pbl_store_get: (vp, va, kp, kc) => this.pbl_store_get(vp, va, kp, kc),
        pbl_store_set: (kp, kc, vp, vc) => this.pbl_store_set(kp, kc, vp, vc),
        pbl_store_key_by_index: (kp, ka, p) => this.pbl_store_key_by_index(kp, ka, p),
      },
    };
    return WebAssembly.instantiate(serial, imports).then(({ module, instance }) => {
      this.instance = instance;
      this.memory = new Uint8Array(this.instance.exports.memory.buffer);
      this.writeRomToClient();
      const err = this.instance.exports.pbl_client_init(
        this.canvas.width, this.canvas.height, this.audio.rate, this.audio.chanc
      );
      if (err < 0) {
        throw new Error(`pbl_client_init: ${err}`);
      }
      this.prevTimeS = Date.now() / 1000 - 0.016;
      this.audio.start();
      this.update();
    });
  }
  
  writeRomToClient() {
    const dstp = this.instance.exports.pbl_client_rom.value;
    const lenp = this.instance.exports.pbl_client_rom_size.value;
    if ((dstp < 0) || (lenp < 0) || (lenp > this.memory.length - 4)) throw new Error(`Invalid ROM storage geometry.`);
    const view = new DataView(this.memory.buffer);
    const lena = view.getInt32(lenp, true);
    if (lena < this.serial.byteLength) {
      throw new Error(`pbl_client_rom too small. ${lena} bytes, require ${this.serial.byteLength}`);
    }
    const cpview = new Uint8Array(this.memory.buffer, dstp, this.serial.byteLength);
    cpview.set(new Uint8Array(this.serial));
    view.setInt32(lenp, this.serial.byteLength, true);
  }
  
  initAudio() {
    const rate = +this.rom.meta["audioRate#"] || 44100;
    const chanc = +this.rom.meta["audioChannels#"] || 1;
    this.audio = new Audio(rate, chanc);
    this.audio.refill = (dst) => {
      let dstp = 0;
      if (!this.terminate && (this.synthLimit > 0)) {
        const srcview = new DataView(this.memory.buffer);
        while (dstp < dst.length) {
          const samplec = Math.min(dst.length - dstp, this.synthLimit);
          let srcp = this.instance.exports.pbl_client_synth(samplec);
          if (!srcp) break;
          for (let i=samplec; i-->0; dstp++, srcp+=2) {
            const sample = srcview.getInt16(srcp, true);
            dst[dstp] = sample / 32768.0;
          }
        }
      }
      while (dstp < dst.length) dst[dstp++] = 0;
    };
  }
  
  initInput() {
    this.input = new Input();
    this.input.onQuit = () => this.terminate = true;
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.canvas = document.createElement("CANVAS");
    const [fbw, fbh] = this.rom.meta.fb.match(/^(\d+)x(\d+)$/).slice(1).map(v => +v);
    if (!fbw || (fbw < 1) || (fbw > 1024) || !fbh || (fbh < 1) || (fbh > 1024)) {
      throw new Error(`Invalid framebuffer size ${JSON.stringify(this.rom.meta.fb)}`);
    }
    this.canvas.width = fbw;
    this.canvas.height = fbh;
    this.canvas.style = `
      width: 100%;
      height: 100%;
      object-fit: contain;
      background-color: #000;
      image-rendering: pixelated;
    `;
    this.element.appendChild(this.canvas);
    // I would rather use "bitmaprenderer", it avoids unnecessary copying, but as of Chrome Linux 127.0.6533.72, it's not working.
    // Does work fine in Firefox. And unfortunately, I don't seem to be able to detect whether it works or fails.
    this.context = this.canvas.getContext("2d", { alpha: false });
  }
  
  selectDefaultLanguage() {
    const user = this.getUserLanguages();
    const game = (this.rom.meta.lang || "").split(',').map(s => s.trim()).filter(s => s);
    for (const lang of game) {
      if (user.includes(lang)) return this.languageCodeFromString(lang);
    }
    return this.languageCodeFromString(game[0] || user[0] || "en");
  }
  
  getUserLanguages() {
    if (navigator?.languages?.length) return navigator.languages.map(l => l.substring(0, 2));
    if (navigator?.language) return [navigator.language];
    return [];
  }
  
  languageCodeFromString(src) {
    if (src.length !== 2) return 0;
    const hi = src.charCodeAt(0);
    const lo = src.charCodeAt(1);
    if ((hi < 0x61) || (hi > 0x7a)) return 0;
    if ((lo < 0x61) || (lo > 0x7a)) return 0;
    return ((hi - 0x61) << 5) | (lo - 0x61);
  }
  
  changeTitlePerRom() {
    document.title = this.rom.getMetaString("title", this.lang);
  }
  
  changeFaviconPerRom() {
    const rid = +this.rom.meta["iconImage#"];
    if (!rid) return;
    const serial = this.rom.getResource(Rom.TID_image, rid);
    if (!serial) return;
    const b64 = util.encodeBase64(serial);
    const element = document.createElement("LINK");
    element.setAttribute("rel", "icon");
    element.setAttribute("href", "data:image/png;base64," + b64);
    for (const old of document.head.querySelectorAll("link[rel='icon']")) old.remove();
    document.head.appendChild(element);
  }
  
  /* Update.
   *****************************************************/
   
  update() {
    if (this.terminate) {
      console.log(`Game terminated with status ${this.termStatus}`);
      this.audio.stop();
      return;
    }
    
    this.input.update(this.instate);
    
    /* Invalid update intervals, force to something sane.
     * Very small (eg high-frequency monitor), skip a frame.
     * Too high, clamp.
     * In a certain sweet range near 60 Hz, pass them thru verbatim.
     */
    const nows = Date.now() / 1000;
    let elapsed = nows - this.prevTimeS;
    if (elapsed <= 0.0) {
      elapsed = 0.016;
    } else if (elapsed < 0.010) {
      window.requestAnimationFrame(() => this.update());
      return;
    } else if (elapsed > 0.032) {
      elapsed = 0.016;
    }
    this.prevTimeS = nows;
    this.instance.exports.pbl_client_update(elapsed, this.instate[0], this.instate[1], this.instate[2], this.instate[3]);
    
    /* Have the client render one frame, and if they do, push it to the canvas.
     */
    const fbp = this.instance.exports.pbl_client_render();
    if (fbp) this.renderFrame(fbp);
    
    //TODO Refill audio

    window.requestAnimationFrame(() => this.update());
  }
  
  renderFrame(fbp) {
    const size = this.canvas.width * this.canvas.height * 4;
    if ((fbp < 0) || (fbp > this.memory.length - size)) return;
    const rgba = new Uint8ClampedArray(this.memory.buffer, fbp, size);
    const imageData = new ImageData(rgba, this.canvas.width, this.canvas.height);
    this.context.putImageData(imageData, 0, 0);
  }
  
  /* Persistence.
   *************************************************/
   
  loadStore() {
    try {
      const store = JSON.parse(localStorage.getItem("pblstore." + this.rom.getPersistenceIdentifier()));
      if (!store || (typeof(store) !== "object")) return {};
      return store;
    } catch (e) {
      return {};
    }
  }
  
  saveStore(store) {
    const serial = JSON.stringify(store);
    localStorage.setItem("pblstore." + this.rom.getPersistenceIdentifier(), serial);
  }
  
  /* Memory access.
   *********************************************************/
   
  memWriteSafe(p, a, src) {
    if (typeof(src) === "string") src = this.textEncoder.encode(src);
    if (src instanceof ArrayBuffer) src = new Uint8Array(src);
    if (!(src instanceof Uint8Array)) throw new Error(`Expected string, ArrayBuffer, or Uint8Array`);
    if (a < 0) a = 0;
    if ((p < 0) || (p > this.memory.length - a)) throw new Error(`Invalid address (${p},${a}) in ${this.memory.length}`);
    const cpc = Math.min(a, src.length);
    const dstview = new Uint8Array(this.memory.buffer, p, cpc);
    dstview.set(src);
    if (src.length < a) this.memory.buffer[src.length] = 0;
    return src.length;
  }
  
  memReadString(p, c) {
    if ((p < 0) || (c < 0) || (p > this.memory.length - c)) {
      throw new Error(`Invalid address (${p},${c}) in ${this.memory.length}`);
    }
    return this.textDecoder.decode(new Uint8Array(this.memory.buffer, p, c));
  }
  
  // Get a nul-terminated string, with fallbacks.
  memReadZString(p) {
    if (p <= 0) return "";
    let c = 0;
    while ((c < 1024) && this.memory[p + c]) c++;
    try {
      return this.textDecoder.decode(new Uint8Array(this.memory.buffer, p, c));
    } catch (e) {
      console.error(e);
      return "";
    }
  }
  
  /* Public API, non-trivial functions.
   * Trivial ones are implemented inline at loadWasm().
   ***************************************************************/
   
  pbl_log(fmtp, vargs) {
    const fmt = this.memReadZString(fmtp);
    const srcview = new DataView(this.memory.buffer);
    let msg = "GAME: ";
    for (let fmtp=0; fmtp<fmt.length; ) {
      const escp = fmt.indexOf("%", fmtp);
      if (escp < 0) {
        msg += fmt.substring(fmtp);
        break;
      }
      msg += fmt.substring(fmtp, escp);
      fmtp = escp + 1;
      switch (fmt[fmtp++]) {
        case '%': msg += "%"; break;
        case 'd': {
            const v = srcview.getInt32(vargs, true);
            vargs += 4;
            msg += v.toString(10);
          } break;
        case 'x': {
            const v = srcview.getInt32(vargs, true);
            vargs += 4;
            msg += v.toString(16);
          } break;
        case 's': {
            const p = srcview.getUint32(vargs, true);
            vargs += 4;
            msg += this.memReadZString(p);
          } break;
        case '.': {
            if ((fmt[fmtp] === '*') && (fmt[fmtp + 1] === 's')) {
              fmtp += 2;
              const c = srcview.getInt32(vargs, true);
              vargs += 4;
              const p = srcview.getUint32(vargs, true);
              vargs += 4;
              msg += this.memReadString(p, c);
            } else {
              msg += fmt.substring(escp, fmtp);
            }
          } break;
        case 'f': {
            if (vargs & 4) vargs += 4;
            const v = srcview.getFloat64(vargs, true);
            vargs += 8;
            msg += v.toString();
          } break;
        case 'p': {
            const v = srcview.getUint32(vargs, true);
            msg += "0x" + v.toString(16);
          } break;
        default: msg += fmt.substring(escp, fmtp);
      }
    }
    console.log(msg);
  }
  
  pbl_now_local(dstp, dsta) {
    if (dsta < 1) return;
    const dst = new DataView(this.memory.buffer, dstp, dsta * 4);
    const now = new Date();
    dst.setInt32(0, now.getFullYear(), true); if (dsta < 2) return;
    dst.setInt32(4, 1 + now.getMonth(), true); if (dsta < 3) return;
    dst.setInt32(8, now.getDate(), true); if (dsta < 4) return;
    dst.setInt32(12, now.getHours(), true); if (dsta < 5) return;
    dst.setInt32(16, now.getMinutes(), true); if (dsta < 6) return;
    dst.setInt32(20, now.getSeconds(), true); if (dsta < 7) return;
    dst.setInt32(24, now.getMilliseconds(), true);
  }
  
  pbl_begin_input_config(playerid) {
    console.log(`pbl_begin_input_config playerid=${playerid}`);
    //TODO Live input config. Punting this a ways.
    return -1;
  }
  
  pbl_store_get(vp, va, kp, kc) {
    const k = this.memReadString(kp, kc);
    const v = this.store[k] || "";
    return this.memWriteSafe(vp, va, v);
  }
  
  pbl_store_set(kp, kc, vp, vc) {
    try {
      const k = this.memReadString(kp, kc);
      const v = this.memReadString(vp, vc);
      //TODO Validation and quota.
      //TODO Debounce store saves, in case the game calls repeatedly.
      this.store[k] = v;
      this.saveStore(this.store);
    } catch (e) {
      console.error(e);
      return -1;
    }
    return 0;
  }
  
  pbl_store_key_by_index(kp, ka, p) {
    if (p < 0) return 0;
    const keys = Object.keys(this.store);
    return this.memWriteSafe(kp, ka, keys[p] || "");
  }
}
