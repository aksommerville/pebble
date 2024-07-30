/* Audio.js
 * Raw PCM delivery to WebAudio, kind of a trick.
 */
 
export class Audio {
  constructor(rate, chanc) {
    this.refill = null; // (Float32Array) => void; Owner must replace.
    this.context = null; // AudioContext
    this.bufferLength = 2048;
    this.rate = rate;
    this.chanc = 1; // More than one channel will be complicated; see AudioBuffer.copyToChannel().
    this.buffers = [{
      raw: new Float32Array(this.bufferLength),
      buffer: null, // AudioBuffer
      node: null, // AudioBufferSourceNode
    }, {
      raw: new Float32Array(this.bufferLength),
      buffer: null, // AudioBuffer
      node: null, // AudioBufferSourceNode
    }];
    this.bufp = 0;
    this.started = false;
    this.nextStartTime = 0;
    this.waitForUserGesture();
  }
  
  waitForUserGesture() {
    //TODO This is crap. There must be some way we can ask "Has the user interacted yet?"
    window.addEventListener("click", () => this.gotUserGesture(), { once: true });
    window.addEventListener("keydown", () => this.gotUserGesture(), { once: true });
  }
  
  gotUserGesture() {
    if (this.context) return;
    this.context = new AudioContext({
      rate: this.rate,
      latencyHint: "interactive",
    });
    if (this.started) this.start();
  }
  
  start() {
    if (!this.context) {
      this.started = true;
      return;
    }
    if (this.context.state === "suspended") {
      this.context.resume();
    }
    for (const b of this.buffers) {
      b.buffer = this.context.createBuffer(this.chanc, this.bufferLength, this.rate);
      b.node = null;
    }
    this.nextStartTime = this.context.currentTime;
    this.update();
  }
  
  stop() {
    if (!this.context) return;
    if (this.context.state !== "suspended") {
      for (const b of this.buffers) {
        if (!b.node) continue;
        b.node.stop(0);
        b.node.disconnect();
        b.node = null;
      }
      this.context.suspend();
    }
  }
  
  update() {
    for (let i=this.buffers.length; i-->0; ) {
      if (this.bufp >= this.buffers.length) this.bufp = 0;
      const b = this.buffers[this.bufp++];
      if (b.node) continue;
      b.node = this.context.createBufferSource();
      b.node.buffer = b.buffer;
      this.refill(b.raw);
      b.buffer.copyToChannel(b.raw, 0);
      b.node.onended = () => {
        b.node.disconnect();
        b.node = null;
        this.update();
      };
      b.node.connect(this.context.destination);
      b.node.start(this.nextStartTime);
      this.nextStartTime += b.buffer.duration;
    }
  }
}
