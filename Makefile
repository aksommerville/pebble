all:
.SILENT:
.SECONDARY:
PRECMD=echo "  $@" ; mkdir -p $(@D) ; 

include etc/config.mk
etc/config.mk:|etc/config.mk.default;$(PRECMD) \
  cp etc/config.mk.default etc/config.mk && \
  echo "Generated build configuration. Please edit etc/config.mk and then rerun make." && \
  exit 1
  
SRCFILES:=$(shell find src -type f)

$(foreach T,$(TARGETS),$(eval include etc/target/$T.mk))

clean:;rm -rf mid out
