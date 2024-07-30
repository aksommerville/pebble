# Build pbltool for this host.

pbltool_MIDDIR:=mid/pbltool
pbltool_OUTDIR:=out/pbltool

pbltool_OPT_ENABLE+=fs image serial

pbltool_CC+=$(patsubst %,-DUSE_%=1,$(pbltool_OPT_ENABLE)) -DPBL_SDK=\"$(abspath .)\"
pbltool_SRCINCLUDE:=$(addprefix src/opt/,$(addsuffix /%,$(pbltool_OPT_ENABLE))) src/pbltool/%
pbltool_SRCFILES:=$(filter $(pbltool_SRCINCLUDE),$(SRCFILES))
pbltool_CFILES:=$(filter %.c,$(pbltool_SRCFILES))
pbltool_OFILES:=$(patsubst src/%.c,$(pbltool_MIDDIR)/%.o,$(pbltool_CFILES))
-include $(pbltool_OFILES:.o=.d)
$(pbltool_MIDDIR)/%.o:src/%.c;$(PRECMD) $(pbltool_CC) -o$@ $<

pbltool_EXE:=$(pbltool_OUTDIR)/pbltool
pbltool-all:$(pbltool_EXE)
$(pbltool_EXE):$(pbltool_OFILES);$(PRECMD) $(pbltool_LD) -o$@ $(pbltool_OFILES) $(pbltool_LDPOST)
