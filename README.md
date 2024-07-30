# pebble

A bare-bones alternative to my game platform Egg.
This is an experiment.
I want the thinnest possible host and host/client interface.

This time, our approach is to dump everything on the client where possible:
- Client supplies a framebuffer of a size negotiated via metadata.
- Client produces a PCM stream on demand. Rate and channel count negotiated via metadata and rejectable.
- Host provides timing and four gamepad states, at each update.
- Host provides language, clock, and persistence.
- ROM file is copied into client memory initially.

So unlike Egg, there is no built-in renderer or synthesizer.

Build cases:
- External web. We supply a friendly load-your-rom web app.
- Embedded web. Single HTML file, no outer chrome. Suitable for iframing.
- External native. We supply a friendly native emulator app.
- Embedded native. Verbatim rom file linked into an executable with the wasm runtime and everything.
- Full native. Game sources compile natively and link against the native runtime directly. ROM assets embed like the embedded case.

## TODO

- [ ] pbltool pack: User-defined compile tools.
- [ ] pbltool bundle, currently stubbed
- [ ] pbltool list
- [ ] pbltool serve
- [x] Onboard wasm-micro-runtime.
- [x] Client API, wasm.
- [ ] Client API, native.
- [x] Web runtime.
- - [x] Input
- [x] Input manager.
- [x] Figure out the audio orchestration.
- [ ] xegl: Use GLES2 or GL2, currently GL1 as an expedient.
- [ ] Remaining Linux drivers: drmgx, bcm, asound, pulse
- [ ] MacOS drivers.
- [ ] Windows drivers.
- [ ] Live input config.
- - [ ] Native
- - [ ] Web
- [ ] Native: Persistence.
- [ ] Native isn't failing due to short pbl_client_rom. Is it even using that?
- [ ] Native: Am I setting PBL_BTN_CD when a keyboard is present? We're supposed to.

## Abandon ...no wait don't

This is cool and all, but I think the audio orchestration can't be solved to my satisfaction.
We'd have to use very smaller buffers on the driver side, and an extra buffer in the runtime, filling up between game updates.
That would cause latency and also introduce a risk of underflow. Worst of both worlds.
(not to mention that implementing a synthesizer in the interpretted WebAssembly code is probably a bad idea for performance).

...oh but we're so close.
Play it out as described just now, see what happens.
...hey you know what? It works just fine, at least in a trivial test.
