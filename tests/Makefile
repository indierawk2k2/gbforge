# Host-native tests for the pure parts of the runtime.
#
# runtime/engine.c has no hardware in it — board state in, event list
# out — so it compiles with the host compiler and runs at full speed.
# That is the difference between checking ten thousand boards and
# checking ten.
#
#   make            build + run everything
#   make transcript compare the C engine against the Python sim

CC      ?= cc
CFLAGS  := -Wall -Wextra -std=gnu11 -O2 -DBANKED= -I../runtime \
           -I../examples/cascadia/generated

BUILD   := build
BOARDS  ?= 10000

.PHONY: all transcript clean

all: transcript

$(BUILD):
	@mkdir -p $@

$(BUILD)/engine_transcript: engine_transcript.c ../runtime/engine.c \
                            ../examples/cascadia/generated/scoring_config.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^

# The oracle: two independent implementations of the same resolver
# must fold to the same 64-bit transcript. A mismatch means the C
# engine and the Python model disagree about the GAME, which no
# screenshot would have told us.
transcript: $(BUILD)/engine_transcript
	@echo "=== C engine ==="
	@$(BUILD)/engine_transcript $(BOARDS) | tee $(BUILD)/c.txt
	@echo "=== Python sim ==="
	@cd .. && python3 -c "from gbforge.engine import sim; sim._transcript($(BOARDS))" \
		| tee tests/$(BUILD)/py.txt
	@if diff -q $(BUILD)/c.txt $(BUILD)/py.txt >/dev/null; then \
		echo "=== transcripts agree over $(BOARDS) boards ==="; \
	else \
		echo "=== TRANSCRIPT MISMATCH ==="; \
		diff $(BUILD)/c.txt $(BUILD)/py.txt; exit 1; \
	fi

clean:
	rm -rf $(BUILD)
