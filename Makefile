ROOT_DIR ?= $(abspath $(CURDIR)/../..)
BUILD_BASE ?= $(ROOT_DIR)/build/apps
BUILD_DIR ?= $(BUILD_BASE)/$(notdir $(CURDIR))
PREFIX ?= $(HOME)/uni/Semester_3/osprakt/arm
OPT_LEVEL ?= -O2

APP_NAME := DOOM
APP_IMAGE := $(BUILD_DIR)/$(APP_NAME).ELF
OBJ_DIR := $(BUILD_DIR)/obj
BUILD_CONFIG := $(BUILD_DIR)/.build-config
LEGACY_BUILD_DIR := $(abspath $(CURDIR)/build)
DOOM_WINDOW_RESX ?= 960
DOOM_WINDOW_RESY ?= 600

CC := $(PREFIX)/bin/arm-none-eabi-gcc

CPPFLAGS += -I$(ROOT_DIR)/cube-os/include -I$(CURDIR)/doomgeneric -I$(ROOT_DIR)/lib/include $(EXTRA_CPPFLAGS) -DCUBEOS \
	-DDOOMGENERIC_RESX=$(DOOM_WINDOW_RESX) -DDOOMGENERIC_RESY=$(DOOM_WINDOW_RESY)
ARCH_FLAGS := -mcpu=cortex-a7 -mfloat-abi=soft -mno-unaligned-access
BASE_CFLAGS := $(OPT_LEVEL) -ggdb -ffreestanding -ffunction-sections \
	-fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables $(ARCH_FLAGS) \
	-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-pointer-sign
CFLAGS += $(BASE_CFLAGS)
DOOM_CFLAGS := -std=gnu23 $(BASE_CFLAGS)
PORT_CFLAGS := -std=gnu23 $(BASE_CFLAGS)
LDFLAGS += $(ARCH_FLAGS) -nostdlib -Wl,--gc-sections -Wl,--fatal-warnings
LDLIBS += -lc -lm -lgcc

DOOMGENERIC_SRCS := \
	$(CURDIR)/doomgeneric/dummy.c \
	$(CURDIR)/doomgeneric/am_map.c \
	$(CURDIR)/doomgeneric/doomdef.c \
	$(CURDIR)/doomgeneric/doomstat.c \
	$(CURDIR)/doomgeneric/dstrings.c \
	$(CURDIR)/doomgeneric/d_event.c \
	$(CURDIR)/doomgeneric/d_items.c \
	$(CURDIR)/doomgeneric/d_iwad.c \
	$(CURDIR)/doomgeneric/d_loop.c \
	$(CURDIR)/doomgeneric/d_main.c \
	$(CURDIR)/doomgeneric/d_mode.c \
	$(CURDIR)/doomgeneric/d_net.c \
	$(CURDIR)/doomgeneric/f_finale.c \
	$(CURDIR)/doomgeneric/f_wipe.c \
	$(CURDIR)/doomgeneric/g_game.c \
	$(CURDIR)/doomgeneric/hu_lib.c \
	$(CURDIR)/doomgeneric/hu_stuff.c \
	$(CURDIR)/doomgeneric/info.c \
	$(CURDIR)/doomgeneric/i_cdmus.c \
	$(CURDIR)/doomgeneric/i_endoom.c \
	$(CURDIR)/doomgeneric/i_joystick.c \
	$(CURDIR)/doomgeneric/i_scale.c \
	$(CURDIR)/doomgeneric/i_sound.c \
	$(CURDIR)/doomgeneric/i_system.c \
	$(CURDIR)/doomgeneric/i_timer.c \
	$(CURDIR)/doomgeneric/memio.c \
	$(CURDIR)/doomgeneric/m_argv.c \
	$(CURDIR)/doomgeneric/m_bbox.c \
	$(CURDIR)/doomgeneric/m_cheat.c \
	$(CURDIR)/doomgeneric/m_config.c \
	$(CURDIR)/doomgeneric/m_controls.c \
	$(CURDIR)/doomgeneric/m_fixed.c \
	$(CURDIR)/doomgeneric/m_menu.c \
	$(CURDIR)/doomgeneric/m_misc.c \
	$(CURDIR)/doomgeneric/m_random.c \
	$(CURDIR)/doomgeneric/p_ceilng.c \
	$(CURDIR)/doomgeneric/p_doors.c \
	$(CURDIR)/doomgeneric/p_enemy.c \
	$(CURDIR)/doomgeneric/p_floor.c \
	$(CURDIR)/doomgeneric/p_inter.c \
	$(CURDIR)/doomgeneric/p_lights.c \
	$(CURDIR)/doomgeneric/p_map.c \
	$(CURDIR)/doomgeneric/p_maputl.c \
	$(CURDIR)/doomgeneric/p_mobj.c \
	$(CURDIR)/doomgeneric/p_plats.c \
	$(CURDIR)/doomgeneric/p_pspr.c \
	$(CURDIR)/doomgeneric/p_saveg.c \
	$(CURDIR)/doomgeneric/p_setup.c \
	$(CURDIR)/doomgeneric/p_sight.c \
	$(CURDIR)/doomgeneric/p_spec.c \
	$(CURDIR)/doomgeneric/p_switch.c \
	$(CURDIR)/doomgeneric/p_telept.c \
	$(CURDIR)/doomgeneric/p_tick.c \
	$(CURDIR)/doomgeneric/p_user.c \
	$(CURDIR)/doomgeneric/r_bsp.c \
	$(CURDIR)/doomgeneric/r_data.c \
	$(CURDIR)/doomgeneric/r_draw.c \
	$(CURDIR)/doomgeneric/r_main.c \
	$(CURDIR)/doomgeneric/r_plane.c \
	$(CURDIR)/doomgeneric/r_segs.c \
	$(CURDIR)/doomgeneric/r_sky.c \
	$(CURDIR)/doomgeneric/r_things.c \
	$(CURDIR)/doomgeneric/sha1.c \
	$(CURDIR)/doomgeneric/sounds.c \
	$(CURDIR)/doomgeneric/statdump.c \
	$(CURDIR)/doomgeneric/st_lib.c \
	$(CURDIR)/doomgeneric/st_stuff.c \
	$(CURDIR)/doomgeneric/s_sound.c \
	$(CURDIR)/doomgeneric/tables.c \
	$(CURDIR)/doomgeneric/v_video.c \
	$(CURDIR)/doomgeneric/wi_stuff.c \
	$(CURDIR)/doomgeneric/w_checksum.c \
	$(CURDIR)/doomgeneric/w_file.c \
	$(CURDIR)/doomgeneric/w_main.c \
	$(CURDIR)/doomgeneric/w_wad.c \
	$(CURDIR)/doomgeneric/z_zone.c \
	$(CURDIR)/doomgeneric/w_file_stdc.c \
	$(CURDIR)/doomgeneric/i_input.c \
	$(CURDIR)/doomgeneric/i_video.c \
	$(CURDIR)/doomgeneric/doomgeneric.c

PORT_C_SRCS := \
	$(CURDIR)/src/doom_cubeos.c \
	$(CURDIR)/src/newlib_stubs.c \
	$(ROOT_DIR)/lib/runtime/crt0.c \
	$(ROOT_DIR)/lib/runtime/perf.c

LOCAL_S_SRCS := $(ROOT_DIR)/lib/runtime/syscall.S

C_SRCS := $(DOOMGENERIC_SRCS) $(PORT_C_SRCS)
S_SRCS := $(LOCAL_S_SRCS)

obj_name = $(OBJ_DIR)/$(subst /,_,$(basename $(patsubst $(ROOT_DIR)/%,%,$(patsubst $(CURDIR)/%,%,$(1))))).o
dep_name = $(patsubst %.o,%.d,$(call obj_name,$(1)))

C_OBJS := $(foreach src,$(C_SRCS),$(call obj_name,$(src)))
S_OBJS := $(foreach src,$(S_SRCS),$(call obj_name,$(src)))
OBJS := $(C_OBJS) $(S_OBJS)
DEPS := $(foreach src,$(C_SRCS) $(S_SRCS),$(call dep_name,$(src)))

.DEFAULT_GOAL := build

-include $(DEPS)

$(BUILD_CONFIG): FORCE
	@mkdir -p $(dir $@)
	@{ \
		printf 'CC=%s\n' '$(CC)'; \
		printf 'CPPFLAGS=%s\n' '$(CPPFLAGS)'; \
		printf 'CFLAGS=%s\n' '$(CFLAGS)'; \
		printf 'DOOM_CFLAGS=%s\n' '$(DOOM_CFLAGS)'; \
		printf 'PORT_CFLAGS=%s\n' '$(PORT_CFLAGS)'; \
		printf 'LDFLAGS=%s\n' '$(LDFLAGS)'; \
		printf 'LDLIBS=%s\n' '$(LDLIBS)'; \
	} > $@.tmp
	@if [ ! -f $@ ] || ! cmp -s $@.tmp $@; then mv -f $@.tmp $@; else rm -f $@.tmp; fi

define register_c_rule
$(call obj_name,$(1)): $(1) $(BUILD_CONFIG)
	@mkdir -p $$(dir $$@)
	$(CC) $(CPPFLAGS) $(if $(filter $(1),$(PORT_C_SRCS)),$(PORT_CFLAGS),$(DOOM_CFLAGS)) -MMD -MP -MF $$(patsubst %.o,%.d,$$@) -c $$< -o $$@
endef

define register_s_rule
$(call obj_name,$(1)): $(1) $(BUILD_CONFIG)
	@mkdir -p $$(dir $$@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF $$(patsubst %.o,%.d,$$@) -c $$< -o $$@
endef

$(foreach src,$(C_SRCS),$(eval $(call register_c_rule,$(src))))
$(foreach src,$(S_SRCS),$(eval $(call register_s_rule,$(src))))

$(APP_IMAGE): $(OBJS) $(BUILD_CONFIG)
	@mkdir -p $(dir $@)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) $(LDLIBS)

.PHONY: FORCE build clean
build: $(APP_IMAGE)

clean:
	rm -rf $(BUILD_DIR)
	@if [ "$(abspath $(BUILD_DIR))" != "$(LEGACY_BUILD_DIR)" ]; then rm -rf "$(LEGACY_BUILD_DIR)"; fi
