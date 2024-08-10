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

Analysis, after completing a game in Pebble for LowRezJam:
- Everything works, there's no fatal flaw preventing Pebble from being a useful platform.
- Requiring render and synth to implement client-side is a performance bottleneck. Definitely going to bite as the games scale up.
- - Upsy-Downsy was conspicuously light on both fronts, render and synth. So we're not seeing just how bad a realistic game will perform.
- Egg will remain the platform of choice. Bring a few Pebble things back into it:
- - Simplify the tooling even further. `pbltool` works great, it should be the model.
- - Reexamine an option for direct audio. Pebble proves it can work. With that, Egg would cover every use case that might otherwise have recommended Pebble.
- - Consider handling ROM transfer the way Pebble does: A single bulk transfer of the whole ROM, and then process it all client-side.
- - Our resource identification regime is fully generic, no need to guess about IDs. Make Egg work like that.

## TODO

- [ ] pbltool serve
- [ ] MacOS drivers.
- [ ] Windows drivers.
- [ ] Live input config.
- - [ ] Native
- - [ ] Web
- [ ] Native: Config file.
- [ ] Useful demo, expose all functionality.
