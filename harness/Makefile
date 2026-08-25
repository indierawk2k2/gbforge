# gbforge verification harness
#
# Builds gbctl: SameBoy's emulator core linked as a library into a
# headless binary that speaks the automation line protocol (see
# PROTOCOL.md) over stdin/stdout. Emulation is PARKED — it advances
# only when a command says so — which is what makes a scripted run
# reproduce frame-for-frame on any machine, CI included.
#
#   make gbctl        build the headless emulator binary
#   make symbols      regenerate the address tables from the ROM's .noi
#   make checkpoints  materialise the savestate cache from recipes
#   make test         run the pytest scenarios (builds everything first)
#
# First run fetches and builds SameBoy (~1 min); later runs are fast.

CC       := clang
CFLAGS   := -Wall -Wextra -std=gnu11 -g -O2 -Iinclude -Wno-unused-parameter

SAMEBOY_TAG := v1.0.3
SAMEBOY_DIR := sameboy
SAMEBOY_LIB := $(SAMEBOY_DIR)/build/lib/libsameboy.a
SAMEBOY_INC := $(SAMEBOY_DIR)/Core
BOOT_ROM    := $(SAMEBOY_DIR)/build/bin/BootROMs/cgb_boot.bin

GAMEDIR   := ../examples/cascadia
ROM       := $(GAMEDIR)/cascadia.gbc
DEBUG_ROM := $(GAMEDIR)/cascadia-debug.gbc
NOI       := $(GAMEDIR)/cascadia.noi

BUILDDIR := build

HARNESS_SRCS := src/harness_core.c src/harness_input.c
HARNESS_OBJS := $(HARNESS_SRCS:src/%.c=$(BUILDDIR)/%.o)
HARNESS_HDRS := include/gbf_harness.h include/gbf_input.h \
                include/gbf_symbols.h

ALLCFLAGS := $(CFLAGS) -I$(SAMEBOY_INC) -I$(SAMEBOY_DIR) \
             -DBOOT_ROM_PATH=\"$(BOOT_ROM)\" -DROM_PATH=\"$(ROM)\" \
             -DGB_DISABLE_REWIND -DGB_DISABLE_DEBUGGER \
             -DGB_DISABLE_CHEAT_SEARCH
ALLLDFLAGS := $(SAMEBOY_LIB) -lc -lm

.PHONY: all gbctl symbols roms checkpoints test clean distclean fetch-sameboy

all: gbctl

# ── SameBoy: fetch source, build the core as a library ──────────────
#
# Pinned to a tag: an emulator that changes under the tests turns a
# behavioural regression into an unexplainable golden-image diff.

$(SAMEBOY_DIR)/Makefile:
	git clone --depth 1 --branch $(SAMEBOY_TAG) \
		https://github.com/LIJI32/SameBoy.git $(SAMEBOY_DIR)

$(SAMEBOY_LIB): | $(SAMEBOY_DIR)/Makefile
	cd $(SAMEBOY_DIR) && $(MAKE) build/lib/libsameboy.a CONF=release \
		DISABLE_REWIND=1 DISABLE_DEBUGGER=1 DISABLE_CHEAT_SEARCH=1

$(BOOT_ROM): | $(SAMEBOY_DIR)/Makefile
	cd $(SAMEBOY_DIR) && $(MAKE) bootroms CONF=release

fetch-sameboy: $(SAMEBOY_LIB) $(BOOT_ROM)

# ── Symbols: the single source of every address ─────────────────────
#
# One parse of the linker's .noi emits the C header (curated ADDR_*
# defines) and pygb/symbols.py (every symbol). Nothing else may name
# an address.

include/gbf_symbols.h: $(NOI) scripts/gen_symbols.py
	python3 scripts/gen_symbols.py $(NOI) $@ \
		--py pygb/symbols.py --json pygb/symbols.json

symbols: include/gbf_symbols.h

$(NOI): roms

roms:
	$(MAKE) -C $(GAMEDIR)
	$(MAKE) -C $(GAMEDIR) debug

# ── gbctl ───────────────────────────────────────────────────────────

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: src/%.c $(HARNESS_HDRS) | $(BUILDDIR) $(SAMEBOY_LIB)
	$(CC) $(ALLCFLAGS) -c -o $@ $<

$(BUILDDIR)/gbctl: src/gbctl.c $(HARNESS_OBJS) $(HARNESS_HDRS) | $(SAMEBOY_LIB)
	$(CC) $(ALLCFLAGS) -o $@ $< $(HARNESS_OBJS) $(ALLLDFLAGS)

gbctl: symbols $(BUILDDIR)/gbctl $(BOOT_ROM)

# ── Checkpoints: savestates as recipes, cache keyed by ROM hash ─────

checkpoints: gbctl
	python3 -m pygb.checkpoints --all

# ── Scenarios ───────────────────────────────────────────────────────

test: gbctl
	python3 -m pytest scenarios -q

clean:
	rm -rf $(BUILDDIR) checkpoints/cache pygb/__pycache__ \
	       scenarios/__pycache__ .pytest_cache

# SameBoy is a fetched dependency, not our source — distclean drops it.
distclean: clean
	rm -rf $(SAMEBOY_DIR)
