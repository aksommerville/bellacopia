all:
.SILENT:

ifeq (,$(EGG_SDK))
  EGG_SDK:=../egg
endif
EGGDEV:=$(EGG_SDK)/out/eggdev

# Each directory under src/tool/ is a custom build-time tool written in C.
# I'll try to borrow as much as possible from Egg, since that has to exist anyway.
include $(EGG_SDK)/local/config.mk
PRECMD=echo "  $@" ; mkdir -p $(@D) ;
TOOLS:=$(notdir $(wildcard src/tool/*))
TOOLS_CFILES:=$(shell find src/tool -name '*.c')
TOOLS_OFILES:=$(patsubst src/tool/%.c,mid/tool/%.o,$(TOOLS_CFILES))
-include $(TOOLS_OFILES:.o=.d)
mid/tool/%.o:src/tool/%.c;$(PRECMD) $(eggdev_CC) -o$@ $< -I$(EGG_SDK)/src/
$(foreach T,$(TOOLS),$(eval \
  out/$T:$$(filter mid/tool/$T/%.o,$(TOOLS_OFILES));$$(PRECMD) $(eggdev_LD) -o$$@ $$^ $(EGG_SDK)/out/$(EGG_NATIVE_TARGET)/libeggrt-headless.a $(eggdev_LDPOST) \
))
TOOL_EXES:=$(patsubst %,out/%,$(TOOLS))
all:$(TOOL_EXES)

# Boilerplate Egg rules.
all:;$(EGGDEV) build
clean:;rm -rf mid out
run:;$(EGGDEV) run
web-run:all;$(EGGDEV) serve --htdocs=out/bellacopia-web.zip --project=.
edit:;$(EGGDEV) serve \
  --htdocs=/data:src/data \
  --htdocs=EGG_SDK/src/web \
  --htdocs=EGG_SDK/src/editor \
  --htdocs=src/editor \
  --htdocs=/synth.wasm:EGG_SDK/out/web/synth.wasm \n  --htdocs=/build:out/bellacopia-web.zip \
  --htdocs=/out:out \
  --writeable=src/data \
  --project=.
