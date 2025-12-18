all:
.SILENT:

ifeq (,$(EGG_SDK))
  EGG_SDK:=../egg
endif
EGGDEV:=$(EGG_SDK)/out/eggdev

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
