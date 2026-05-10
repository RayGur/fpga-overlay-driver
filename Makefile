# ─────────────────────────────────────────────────────────
# fpga-overlay-driver Makefile
#
# Targets:
#   make              — build main overlay binary
#   make tests        — build all test binaries
#   make clean        — remove build artifacts
#
# Platform:
#   make              — default: Ultrascale+ (KV260, uint64_t address)
#   make PLATFORM=Z2  — PYNQ-Z2 (Zynq-7000, uint32_t address)
# ─────────────────────────────────────────────────────────

CC      := gcc
CFLAGS  := -Wall -Wextra -g -I./include
TARGET  := overlay

# Platform selection
ifeq ($(PLATFORM), Z2)
    CFLAGS += -DZYNQ7000
    $(info [config] Platform: PYNQ-Z2 (Zynq-7000, 32-bit address))
else
    $(info [config] Platform: Ultrascale+ / default (64-bit address))
endif

# Source files for main binary
SRCS := src/main.c      \
        src/bit2bin.c   \
        src/hwh_parser.c \
        src/fpga_load.c \
        src/mmio.c

OBJS := $(SRCS:.c=.o)

# ─── Main binary ──────────────────────────────────────────

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "[✓] Built: $(TARGET)"

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# ─── Test binaries ────────────────────────────────────────

TEST_BIT2BIN    := test/test_bit2bin
TEST_HWH_PARSER := test/test_hwh_parser

.PHONY: tests
tests: $(TEST_BIT2BIN) $(TEST_HWH_PARSER)
	@echo "[✓] Built all tests"

$(TEST_BIT2BIN): test/test_bit2bin.c src/bit2bin.c
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_HWH_PARSER): test/test_hwh_parser.c src/hwh_parser.c
	$(CC) $(CFLAGS) -o $@ $^ -lexpat

# ─── Clean ────────────────────────────────────────────────

.PHONY: clean
clean:
	rm -f src/*.o
	rm -f $(TARGET)
	rm -f $(TEST_BIT2BIN) $(TEST_HWH_PARSER)
	@echo "[✓] Cleaned"
