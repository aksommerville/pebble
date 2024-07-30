linux_MIDDIR:=mid/linux
linux_OUTDIR:=out/linux

linux_OPT_ENABLE+=fs strfmt serial hostio

linux_CC+=$(patsubst %,-DUSE_%=1,$(linux_OPT_ENABLE))
# Linux optional units: evdev, alsafd,asound,pulse, bcm,drmgx,glx,xegl
ifneq (,$(strip $(filter asound,$(linux_OPT_ENABLE))))
  linux_LDPOST+=-lasound
endif
ifneq (,$(strip $(filter pulse,$(linux_OPT_ENABLE))))
  linux_LDPOST+=-lpulse-simple
endif
ifneq (,$(strip $(filter bcm,$(linux_OPT_ENABLE))))
  #TODO Extra includes and libs for Broadcom VideoCore.
endif
ifneq (,$(strip $(filter drmgx glx xegl,$(linux_OPT_ENABLE))))
  linux_LDPOST+=-lGL
endif
ifneq (,$(strip $(filter drmgx xegl,$(linux_OPT_ENABLE))))
  linux_LDPOST+=-lEGL
endif
ifneq (,$(strip $(filter glx xegl,$(linux_OPT_ENABLE))))
  linux_LDPOST+=-lX11
  ifneq (,$(strip $(filter xinerama,$(linux_OPT_ENABLE))))
    linux_LDPOST+=-lXinerama
  endif
endif

linux_CC+=-I$(WAMR_SDK)/core/iwasm/include
linux_LDPOST+=$(WAMR_SDK)/build/libvmlib.a -lpthread

linux_SRCINCLUDE:=$(addprefix src/opt/,$(addsuffix /%,$(linux_OPT_ENABLE))) src/pblrt/%

# These two files are special, they each get built two different ways.
linux_NOTFILES:=$(linux_MIDDIR)/pblrt/pblrt_romsrc.o $(linux_MIDDIR)/pblrt/pblrt_exec.o
linux_ROMSRC_EMBEDDED:=$(linux_MIDDIR)/pblrt/pblrt_romsrc_embedded.o
linux_ROMSRC_EXTERNAL:=$(linux_MIDDIR)/pblrt/pblrt_romsrc_external.o
linux_EXEC_WASM:=$(linux_MIDDIR)/pblrt/pblrt_exec_wasm.o
linux_EXEC_NATIVE:=$(linux_MIDDIR)/pblrt/pblrt_exec_native.o
linux_OFILES_CONDITIONAL:=$(linux_ROMSRC_EMBEDDED) $(linux_ROMSRC_EXTERNAL) $(linux_EXEC_WASM) $(linux_EXEC_NATIVE)
  
linux_SRCFILES:=$(filter $(linux_SRCINCLUDE),$(SRCFILES))
linux_CFILES:=$(filter %.c,$(linux_SRCFILES))
linux_OFILES:=$(filter-out $(linux_NOTFILES),$(patsubst src/%.c,$(linux_MIDDIR)/%.o,$(linux_CFILES)))
-include $(patsubst %.o,%.d,$(linux_OFILES) $(linux_OFILES_CONDITIONAL))

$(linux_MIDDIR)/%.o:src/%.c;$(PRECMD) $(linux_CC) -o$@ $<
$(linux_ROMSRC_EMBEDDED):src/pblrt/pblrt_romsrc.c;$(PRECMD) $(linux_CC) -o$@ $< -DROMSRC=EMBEDDED
$(linux_ROMSRC_EXTERNAL):src/pblrt/pblrt_romsrc.c;$(PRECMD) $(linux_CC) -o$@ $< -DROMSRC=EXTERNAL
$(linux_EXEC_WASM):src/pblrt/pblrt_exec.c;$(PRECMD) $(linux_CC) -o$@ $< -DEXECFMT=WASM
$(linux_EXEC_NATIVE):src/pblrt/pblrt_exec.c;$(PRECMD) $(linux_CC) -o$@ $< -DEXECFMT=NATIVE

# pebble: The general ROM file executor.
linux_EXE_PEBBLE:=$(linux_OUTDIR)/pebble
linux-all:$(linux_EXE_PEBBLE)
linux_OFILES_EXE_PEBBLE:=$(linux_OFILES) $(linux_ROMSRC_EXTERNAL) $(linux_EXEC_WASM)
$(linux_EXE_PEBBLE):$(linux_OFILES_EXE_PEBBLE);$(PRECMD) $(linux_LD) -o$@ $(linux_OFILES_EXE_PEBBLE) $(linux_LDPOST)

# libpebblenative.a: Full runtime and execution wrapper, expecting to link against a native Pebble game.
linux_LIB_TRUE:=$(linux_OUTDIR)/libpebblenative.a
linux-all:$(linux_LIB_TRUE)
linux_OFILES_LIB_TRUE:=$(linux_OFILES) $(linux_ROMSRC_EMBEDDED) $(linux_EXEC_NATIVE)
$(linux_LIB_TRUE):$(linux_OFILES_LIB_TRUE);$(PRECMD) $(linux_AR) rc $@ $(linux_OFILES_LIB_TRUE)

# libpebblefake.a: Full runtime and execution wrapper, expecting to link against an embedded Pebble ROM.
linux_LIB_FAKE:=$(linux_OUTDIR)/libpebblefake.a
linux-all:$(linux_LIB_FAKE)
linux_OFILES_LIB_FAKE:=$(linux_OFILES) $(linux_ROMSRC_EMBEDDED) $(linux_EXEC_WASM)
$(linux_LIB_FAKE):$(linux_OFILES_LIB_FAKE);$(PRECMD) $(linux_AR) rc $@ $^

ifeq (,Native game executables. Copied from eggsc. Get to this when we get to it.)
# demo-fake: Fake-native executable of our demo ROM.
linux_DEMO_FAKE:=$(linux_OUTDIR)/demo-fake
linux-all:$(linux_DEMO_FAKE)
linux_DEMO_FAKE_O:=$(linux_MIDDIR)/demo-fake.o
linux_DEMO_FAKE_S:=$(linux_MIDDIR)/demo-fake.s
$(linux_DEMO_FAKE_S):;$(PRECMD) echo \
  '.globl eggzek_embedded_rom,eggzek_embedded_rom_size\n' \
  'eggzek_embedded_rom:\n' \
  '.incbin "'$(demos_ROM)'"\n' \
  'eggzek_embedded_rom_end:\n' \
  'eggzek_embedded_rom_size:.int (eggzek_embedded_rom_end-eggzek_embedded_rom)' > $@
$(linux_DEMO_FAKE_O):$(demos_ROM) $(linux_DEMO_FAKE_S);$(PRECMD) $(linux_CC) -xassembler-with-cpp -o$@ $(linux_DEMO_FAKE_S)
$(linux_DEMO_FAKE):$(linux_LIB_FAKE) $(linux_DEMO_FAKE_O);$(PRECMD) $(linux_LD) -o$@ $^ $(linux_LDPOST)

# demo-true: True-native executable of our demo ROM.
linux_DEMO_TRUE:=$(linux_OUTDIR)/demo-true
linux-all:$(linux_DEMO_TRUE)
linux_DEMO_TRUE_ROM:=$(linux_MIDDIR)/demo-true.egg
$(linux_DEMO_TRUE_ROM):$(demos_ROM) $(tools_EXE_EGGDEV);$(PRECMD) $(tools_EXE_EGGDEV) stripcode -o$@ $<
linux_DEMO_TRUE_GAME_CFILES:=$(filter src/demo/src/%.c,$(SRCFILES))
linux_DEMO_TRUE_GAME_OFILES:=$(patsubst src/demo/src/%.c,$(linux_MIDDIR)/demo/%.o,$(linux_DEMO_TRUE_GAME_CFILES))
-include $(linux_DEMO_TRUE_GAME_OFILES:.o=.d)
$(linux_MIDDIR)/demo/%.o:src/demo/src/%.c;$(PRECMD) $(linux_CC) -o$@ $< -DUSE_REAL_STDLIB=1
linux_DEMO_TRUE_O:=$(linux_MIDDIR)/demo-true.o
linux_DEMO_TRUE_S:=$(linux_MIDDIR)/demo-true.s
$(linux_DEMO_TRUE_S):;$(PRECMD) echo \
  '.globl eggzek_embedded_rom,eggzek_embedded_rom_size\n' \
  'eggzek_embedded_rom:\n' \
  '.incbin "'$(linux_DEMO_TRUE_ROM)'"\n' \
  'eggzek_embedded_rom_end:\n' \
  'eggzek_embedded_rom_size:.int (eggzek_embedded_rom_end-eggzek_embedded_rom)' > $@
$(linux_DEMO_TRUE_O):$(linux_DEMO_TRUE_ROM) $(linux_DEMO_TRUE_S);$(PRECMD) $(linux_CC) -xassembler-with-cpp -o$@ $(linux_DEMO_TRUE_S)
linux_DEMO_TRUE_OFILES:=$(filter-out \
  $(linux_MIDDIR)/eggzek/eggzek_romsrc_external.o \
  $(linux_MIDDIR)/eggrt/cpu/% \
,$(linux_OFILES)) $(linux_MIDDIR)/eggrt/cpu/eggrt_cpu_native.o $(linux_DEMO_TRUE_O) $(linux_DEMO_TRUE_GAME_OFILES)
linux_DEMO_TRUE_LDPOST:=$(filter-out %libvmlib.a,$(linux_LDPOST))
$(linux_DEMO_TRUE):$(linux_DEMO_TRUE_OFILES);$(PRECMD) $(linux_LD) -o$@ $^ $(linux_DEMO_TRUE_LDPOST)
endif

linux-run:$(linux_EXE_PEBBLE) $(demo_ROM);$(linux_EXE_PEBBLE) $(demo_ROM)
