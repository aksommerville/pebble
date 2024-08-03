# Build pbltool for this host.

pbltool_MIDDIR:=mid/pbltool
pbltool_OUTDIR:=out/pbltool

pbltool_OPT_ENABLE+=fs image serial midi

pbltool_LATE_BINDING_DEFINES= \
  '-DPBL_LD="$($(NATIVE_TARGET)_LD)"' \
  '-DPBL_LDPOST="$($(NATIVE_TARGET)_LDPOST)"' \
  '-DPBL_CC="$($(NATIVE_TARGET)_CC)"'

pbltool_CC+=$(patsubst %,-DUSE_%=1,$(pbltool_OPT_ENABLE)) '-DPBL_SDK="$(abspath .)"' '-DPBL_NATIVE_TARGET="$(NATIVE_TARGET)"'
pbltool_SRCINCLUDE:=$(addprefix src/opt/,$(addsuffix /%,$(pbltool_OPT_ENABLE))) src/pbltool/%
pbltool_SRCFILES:=$(filter $(pbltool_SRCINCLUDE),$(SRCFILES))
pbltool_CFILES:=$(filter %.c,$(pbltool_SRCFILES))
pbltool_OFILES:=$(patsubst src/%.c,$(pbltool_MIDDIR)/%.o,$(pbltool_CFILES))
-include $(pbltool_OFILES:.o=.d)
$(pbltool_MIDDIR)/%.o:src/%.c;$(PRECMD) $(pbltool_CC) -o$@ $< $(pbltool_LATE_BINDING_DEFINES)

pbltool_EXE:=$(pbltool_OUTDIR)/pbltool
pbltool-all:$(pbltool_EXE)
$(pbltool_EXE):$(pbltool_OFILES);$(PRECMD) $(pbltool_LD) -o$@ $(pbltool_OFILES) $(pbltool_LDPOST)
