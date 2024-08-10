demo_MIDDIR:=mid/demo
demo_OUTDIR:=out/demo

demo_SHARE_ENABLE:=stdlib

demo_CC+=$(patsubst %,-DUSE_%=1,$(demo_SHARE_ENABLE))
demo_CC_NATIVE+=$(patsubst %,-DUSE_%=1,$(demo_SHARE_ENABLE))

demo_SRCINCLUDE:=$(addprefix src/share/,$(addsuffix /%,$(demo_OPT_ENABLE))) src/demo/%
demo_SRCFILES:=$(filter $(demo_SRCINCLUDE),$(SRCFILES))
demo_CFILES:=$(filter %.c,$(demo_SRCFILES))
demo_OFILES:=$(patsubst src/%.c,$(demo_MIDDIR)/%.o,$(demo_CFILES))
-include $(demo_OFILES:.o=.d)
$(demo_MIDDIR)/%.o:src/%.c;$(PRECMD) $(demo_CC) -o$@ $<

demo_CODE1:=$(demo_MIDDIR)/data/code.wasm
$(demo_CODE1):$(demo_OFILES);$(PRECMD) $(demo_LD) -o$@ $^ $(demo_LDPOST)

demo_DATAFILES:=$(filter src/demo/data/%,$(demo_SRCFILES))
demo_DATADIRS:=$(sort $(dir $(demo_DATAFILES)))
ifneq (,$(WAMR_SDK))
# If no WAMR, we'll still build a ROM but without code:1, so it's not valid without accompanying native code.
demo_DATAFILES+=$(demo_CODE1)
demo_PACK_INPUTS:=$(demo_MIDDIR)/data
endif

demo_ROM:=$(demo_OUTDIR)/demo.pbl
demo-all:$(demo_ROM)
$(demo_ROM):$(demo_DATAFILES) $(pbltool_EXE);$(PRECMD) $(pbltool_EXE) pack -o$@ src/demo/data $(demo_PACK_INPUTS)

demo_HTML:=$(demo_OUTDIR)/demo.html
demo-all:$(demo_HTML)
$(demo_HTML):$(demo_ROM) $(pbltool_EXE);$(PRECMD) $(pbltool_EXE) bundle -o$@ $<
