# Cascadia — builds the example ROM from the spec.
#
#   make gen     regenerate generated/ from cascadia.py (committed)
#   make         compile + link cascadia.gbc
#   make debug   dev ROM with the scriptable mailbox compiled in
#
# Requires GBDK-2020 (https://github.com/gbdk-2020/gbdk-2020),
# release 4.5.0 or later. Point GBDK_HOME at its root.

GBDK_HOME ?= ../../../gbdk/
LCC = $(GBDK_HOME)bin/lcc

ROOT = ../..
RUNTIME = $(ROOT)/runtime
GEN = generated
RES = res
OBJ = obj$(if $(DEBUG),_debug,)
DEBUGFLAGS = $(if $(DEBUG),-DDEBUG_BUILD,)

LCCFLAGS = -Wm-yC -Wm-yt0x1B -Wm-yo32 -Wm-ya1 -Wl-j -Wl-m $(DEBUGFLAGS)
INC = -I. -I$(RUNTIME) -I$(GEN) -I$(RES)

RUNTIME_OBJS = engine vram anim flow ui_core hud title ability \
               reset cursor debug sound_glue_stub
GEN_OBJS = mode_config ui_overlays anim_tables title_config
RES_OBJS = tiles_data sprites_data palettes

OBJS = $(OBJ)/main_cascadia.o \
       $(RUNTIME_OBJS:%=$(OBJ)/%.o) \
       $(GEN_OBJS:%=$(OBJ)/%.o) \
       $(RES_OBJS:%=$(OBJ)/%.o)

all: prepare cascadia.gbc

debug:
	$(MAKE) DEBUG=1 all

gen:
	cd $(ROOT) && python3 -m gbforge build examples/cascadia \
		-o examples/cascadia/$(GEN)

res:
	python3 $(ROOT)/scripts/gen_placeholder_res.py $(RES) --logo CASCADIA

prepare:
	@mkdir -p $(OBJ)

$(OBJ)/%.o: %.c
	$(LCC) $(LCCFLAGS) $(INC) -c -o $@ $<

$(OBJ)/%.o: $(RUNTIME)/%.c
	$(LCC) $(LCCFLAGS) $(INC) -c -o $@ $<

$(OBJ)/%.o: $(GEN)/%.c
	$(LCC) $(LCCFLAGS) $(INC) -c -o $@ $<

$(OBJ)/%.o: $(RES)/%.c
	$(LCC) $(LCCFLAGS) $(INC) -c -o $@ $<

cascadia.gbc: $(OBJS)
	$(LCC) $(LCCFLAGS) -o $@ $(OBJS)
	python3 $(ROOT)/scripts/check_banks.py cascadia.noi

clean:
	rm -rf obj obj_debug

.PHONY: all debug gen res prepare clean
