# Mini Minecraft GBA - build with devkitARM (https://devkitpro.org)
#
#   make          build MiniMinecraft.gba
#   make clean    remove build output
#   make test     run the PC-side logic tests (needs only gcc)
#   make shots    render test screenshots to build/shots (needs python3 + Pillow)

TARGET    := MiniMinecraft
# ROM header fields (title max 12 chars, game code 4 chars)
TITLE     := MINIMCGBA
GAMECODE  := BMMC
MAKER     := 00

DEVKITPRO ?= /opt/devkitpro
DEVKITARM ?= $(DEVKITPRO)/devkitARM

PREFIX  := $(DEVKITARM)/bin/arm-none-eabi-
CC      := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
GBAFIX  := $(firstword $(wildcard $(DEVKITPRO)/tools/bin/gbafix) $(shell command -v gbafix 2>/dev/null))

BUILD   := build
SOURCES := source/main.c source/game.c source/art.c
OBJS    := $(SOURCES:%.c=$(BUILD)/%.o)

ARCH    := -mthumb -mthumb-interwork
CFLAGS  := $(ARCH) -O2 -Wall -Wextra -std=gnu99 -ffunction-sections -fdata-sections -Iinclude -Isource
LDFLAGS := $(ARCH) -specs=gba.specs -Wl,--gc-sections

.PHONY: all clean test shots check-toolchain

all: check-toolchain $(TARGET).gba

check-toolchain:
	@test -x "$(CC)" || { echo "error: devkitARM not found at $(DEVKITARM)."; \
	  echo "Install devkitPro (https://devkitpro.org/wiki/Getting_Started) or set DEVKITARM / DEVKITPRO."; \
	  echo "Docker alternative:  docker run --rm -v \"\$$PWD\":/work -w /work devkitpro/devkitarm make"; \
	  exit 1; }

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(TARGET).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(TARGET).gba: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
ifneq ($(GBAFIX),)
	$(GBAFIX) $@ -t$(TITLE) -c$(GAMECODE) -m$(MAKER)
else
	@echo "warning: gbafix not found - ROM header checksum not fixed."
	@echo "         Emulators don't care; real hardware / flash carts need it (pacman -S gba-tools)."
endif
	@echo "Built $@ ($$(wc -c < $@) bytes)"

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).gba

# ---- PC-side tests (no GBA toolchain needed) -------------------------------
$(BUILD)/host_test: tests/host_test.c source/game.c source/art.c include/gba.h source/game.h source/art.h
	@mkdir -p $(BUILD)
	gcc -std=c99 -Wall -Wextra -pedantic -DHOST_TEST -Iinclude tests/host_test.c source/game.c source/art.c -o $@

test: $(BUILD)/host_test
	@mkdir -p $(BUILD)/dump
	$(BUILD)/host_test $(BUILD)/dump

shots: test
	@mkdir -p $(BUILD)/shots
	@for n in title play pause corner; do python3 tests/render.py $(BUILD)/dump $$n $(BUILD)/shots/$$n.png 3; done
	@echo "Screenshots in $(BUILD)/shots"

-include $(OBJS:.o=.d)
