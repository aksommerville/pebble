/* Input.js
 * Simple input manager.
 * Reads from keyboard and gamepads, writes to 4 Pebble device states.
 */
 
export class Input {
  constructor() {
  
    // Owner may override:
    this.onQuit = () => {};
    
    this.btnidByKeyCode = {
      ArrowLeft: Input.BTN_LEFT,
      ArrowRight: Input.BTN_RIGHT,
      ArrowUp: Input.BTN_UP,
      ArrowDown: Input.BTN_DOWN,
      KeyZ: Input.BTN_SOUTH,
      KeyX: Input.BTN_WEST,
      KeyC: Input.BTN_EAST,
      KeyV: Input.BTN_NORTH,
      KeyW: Input.BTN_UP,
      KeyA: Input.BTN_LEFT,
      KeyS: Input.BTN_DOWN,
      KeyD: Input.BTN_RIGHT,
      Space: Input.BTN_SOUTH,
      Comma: Input.BTN_WEST,
      Period: Input.BTN_EAST,
      Slash: Input.BTN_NORTH,
      Enter: Input.BTN_AUX1,
      BracketRight: Input.BTN_AUX2,
      BracketLeft: Input.BTN_AUX1,
      Tab: Input.BTN_L1,
      Backslash: Input.BTN_R1,
      Backquote: Input.BTN_L2,
      Backspace: Input.BTN_R2,
      Escape: Input.SIG_QUIT,
    };
    
    // We're going to assume that all gamepads use "standard" mapping, and also not touch the axes.
    //TODO We can do better than that.
    this.btnidForStandardGamepad = [
      Input.BTN_SOUTH,
      Input.BTN_EAST,
      Input.BTN_WEST,
      Input.BTN_NORTH,
      Input.BTN_L1,
      Input.BTN_R1,
      Input.BTN_L2,
      Input.BTN_R2,
      Input.BTN_AUX2,
      Input.BTN_AUX1,
      0, // LP
      0, // RP
      Input.BTN_UP,
      Input.BTN_DOWN,
      Input.BTN_LEFT,
      Input.BTN_RIGHT,
      Input.BTN_AUX3,
    ];
    
    this.keyState = Input.BTN_CD;
    this.gamepads = []; // Sparse; indexed by gamepad.index
    this.playeridNext = 1;
    window.addEventListener("gamepadconnected", e => this.onGamepad(e));
    window.addEventListener("gamepaddisconnected", e => this.onGamepad(e));
    window.addEventListener("keydown", e => this.onKey(e));
    window.addEventListener("keyup", e => this.onKey(e));
  }
  
  update(dstv) {
    this.updateGamepads(dstv);
    dstv[0] = this.keyState;
    for (const gp of this.gamepads) {
      if (!gp) continue;
      dstv[gp.playerid - 1] |= gp.state;
    }
  }
  
  /* Gamepad.
   ********************************************************/
   
  onGamepad(event) {
    if (event.type === "gamepadconnected") {
      const playerid = this.playeridNext++;
      if (this.playeridNext > 4) this.playeridNext = 1;
      this.gamepads[event.gamepad.index] = {
        id: event.gamepad.id,
        buttons: event.gamepad.buttons?.map(b => 0),
        state: Input.BTN_CD,
        playerid,
      };
    } else {
      delete this.gamepads[event.gamepad.index];
    }
  }
  
  updateGamepads(dstv) {
    if (!navigator.getGamepads) return;
    for (const src of navigator.getGamepads()) {
      if (!src) continue;
      const gp = this.gamepads[src.index];
      if (!gp) continue;
      for (let i=src.buttons.length; i-->0; ) {
        if (src.buttons[i].value) {
          if (gp.buttons[i]) continue;
          gp.buttons[i] = 1;
          gp.state |= this.btnidForStandardGamepad[i];
        } else {
          if (!gp.buttons[i]) continue;
          gp.buttons[i] = 0;
          gp.state &= ~this.btnidForStandardGamepad[i];
        }
      }
    }
  }
  
  /* Keyboard.
   *****************************************************/
   
  onKey(event) {
    if (event.ctrlKey || event.altKey) return; // Don't interfere.
    const btnid = this.btnidByKeyCode[event.code];
    if (!btnid) return; // Don't interfere.
    event.preventDefault();
    event.stopPropagation();
    
    if (event.type === "keyup") {
      if (btnid > 0xffff) return;
      if (!(this.keyState & btnid)) return;
      this.keyState &= ~btnid;
      
    } else {
      if (btnid > 0xffff) {
        switch (btnid) {
          case Input.SIG_QUIT: this.onQuit(); break;
        }
        return;
      }
      if (this.keyState & btnid) return;
      this.keyState |= btnid;
    }
  }
}

Input.BTN_LEFT     = 0x0001;
Input.BTN_RIGHT    = 0x0002;
Input.BTN_UP       = 0x0004;
Input.BTN_DOWN     = 0x0008;
Input.BTN_SOUTH    = 0x0010;
Input.BTN_WEST     = 0x0020;
Input.BTN_EAST     = 0x0040;
Input.BTN_NORTH    = 0x0080;
Input.BTN_L1       = 0x0100;
Input.BTN_R1       = 0x0200;
Input.BTN_L2       = 0x0400;
Input.BTN_R2       = 0x0800;
Input.BTN_AUX1     = 0x1000;
Input.BTN_AUX2     = 0x2000;
Input.BTN_AUX3     = 0x4000;
Input.BTN_CD       = 0x8000;

Input.SIG_QUIT     = 0x10001;
