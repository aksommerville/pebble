# pebble

A bare-bones alternative to my game platform Egg.
This is an experiment.
I want the thinnest possible host and host/client interface.

This time, our approach is to dump everything on the client where possible:
- Client supplies a framebuffer of a size negotiated via metadata.
- Client produces a PCM stream on demand. Rate and channel count negotiated via metadata and rejectable.
- Host provides timing and four gamepad states, at each update.
- Host provides language, clock, and persistence.
- ~ROM file is copied into client memory initially.~ Client must call for it.

So unlike Egg, there is no built-in renderer or synthesizer.

Build cases:
- External web. We supply a friendly load-your-rom web app.
- Embedded web. Single HTML file, no outer chrome. Suitable for iframing.
- External native. We supply a friendly native emulator app.
- Embedded native. Verbatim rom file linked into an executable with the wasm runtime and everything.
- Full native. Game sources compile natively and link against the native runtime directly. ROM assets embed like the embedded case.

## TODO

- [x] pbltool pack: User-defined resource compilers.
- - I think we can skip this? Force clients to build up a secondary intermediate set if they need pre-processing.
- [x] pbltool unbundle html: Capture icon from `<link>` if present.
- [ ] pbltool serve
- [ ] xegl: Use GLES2 or GL2, currently GL1 as an expedient.
- [ ] Remaining Linux drivers: drmgx, bcm, asound, pulse
- [ ] MacOS drivers.
- [ ] Windows drivers.
- [ ] Live input config.
- - [ ] Native
- - [ ] Web
- [ ] Native: Config file.
- [x] Native: app icon
- [ ] Useful demo, expose all functionality.
- [x] Try building a game outside the auspices of this repo. An entry for Lowrez Jam 2024, which happened to be starting at the right moment. :)
- - https://github.com/aksommerville/upsy-downsy ; works pretty nice!
