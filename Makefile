ROOT_DIR ?= $(abspath $(CURDIR)/../..)
BUILD_BASE ?= $(ROOT_DIR)/build/apps
BUILD_DIR ?= $(BUILD_BASE)/$(notdir $(CURDIR))
NEWLIB_SYSROOT ?=
OPT_LEVEL ?= -O3
CONFIG_HEADER ?= $(ROOT_DIR)/cube-os/include/config.h

APP_NAME := DOOM
APP_IMAGE := $(BUILD_DIR)/$(APP_NAME).ELF
OBJ_DIR := $(BUILD_DIR)/obj
USER_PERF_ENABLED ?= $(shell if grep -Eq '^[[:space:]]*#define[[:space:]]+LOG_PERFORMANCE_USER([[:space:]]|$$)' "$(CONFIG_HEADER)"; then printf '1'; else printf '0'; fi)
USER_PERF_RUNTIME_SRC := $(ROOT_DIR)/lib/runtime/perf.c

CC := arm-none-eabi-gcc
READELF := arm-none-eabi-readelf
UPERF_SYMBOL_BLOBS_SRC := $(ROOT_DIR)/lib/runtime/perf_symbol_blobs.S

GCC_DEFAULT_SYSROOT := $(strip $(shell $(CC) -print-sysroot 2>/dev/null))
LOCAL_NEWLIB_SYSROOT := $(ROOT_DIR)/newlib/arm-none-eabi
EFFECTIVE_NEWLIB_SYSROOT := $(firstword $(foreach dir,$(NEWLIB_SYSROOT) $(LOCAL_NEWLIB_SYSROOT) $(GCC_DEFAULT_SYSROOT),$(if $(and $(strip $(dir)),$(wildcard $(dir)/include/stdio.h),$(wildcard $(dir)/include/strings.h),$(wildcard $(dir)/lib/libc.a),$(wildcard $(dir)/lib/libm.a)),$(abspath $(dir)))))
ifneq ($(EFFECTIVE_NEWLIB_SYSROOT),)
SYSROOT_FLAGS := --sysroot=$(EFFECTIVE_NEWLIB_SYSROOT)
endif

CPPFLAGS += $(SYSROOT_FLAGS) -I$(ROOT_DIR)/cube-os/include -I$(CURDIR)/doomgeneric -I$(ROOT_DIR)/lib/include $(EXTRA_CPPFLAGS) -DCUBEOS
ARCH_FLAGS := -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=softfp -mno-unaligned-access
BASE_CFLAGS := $(OPT_LEVEL) -ggdb -ffreestanding -ffunction-sections \
	-fno-builtin-memcpy -fno-builtin-memmove -fno-builtin-memset -fno-builtin-memcmp \
	-fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables $(ARCH_FLAGS) \
	-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-pointer-sign
USER_PROFILER_CFLAGS := $(if $(filter 1,$(USER_PERF_ENABLED)),-finstrument-functions)
CFLAGS += $(BASE_CFLAGS) $(USER_PROFILER_CFLAGS)
LDFLAGS += $(SYSROOT_FLAGS) $(ARCH_FLAGS) -nostdlib -Wl,--gc-sections -Wl,--fatal-warnings
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
	$(CURDIR)/doomgeneric/i_input.c \
	$(CURDIR)/doomgeneric/i_video.c \
	$(CURDIR)/doomgeneric/doomgeneric.c

PORT_C_SRCS := \
	$(ROOT_DIR)/lib/core/mem.c \
	$(CURDIR)/src/doom_cubeos.c \
	$(CURDIR)/src/w_file_cubeos.c \
	$(CURDIR)/src/newlib_stubs.c \
	$(ROOT_DIR)/lib/runtime/crt0.c
ifeq ($(USER_PERF_ENABLED),1)
PORT_C_SRCS += $(USER_PERF_RUNTIME_SRC)
endif

LOCAL_S_SRCS := $(ROOT_DIR)/lib/runtime/syscall.S

C_SRCS := $(DOOMGENERIC_SRCS) $(PORT_C_SRCS)
S_SRCS := $(LOCAL_S_SRCS)

obj_name = $(OBJ_DIR)/$(subst /,_,$(basename $(patsubst $(ROOT_DIR)/%,%,$(patsubst $(CURDIR)/%,%,$(1))))).o
dep_name = $(patsubst %.o,%.d,$(call obj_name,$(1)))

C_OBJS := $(foreach src,$(C_SRCS),$(call obj_name,$(src)))
S_OBJS := $(foreach src,$(S_SRCS),$(call obj_name,$(src)))
OBJS := $(C_OBJS) $(S_OBJS)
UPERF_PRELINK_IMAGE := $(BUILD_DIR)/$(APP_NAME).preperf.ELF
UPERF_SYMTAB_BIN := $(BUILD_DIR)/user_perf_symtab.bin
UPERF_STRTAB_BIN := $(BUILD_DIR)/user_perf_strtab.bin
UPERF_SYMBOLS_OBJ := $(BUILD_DIR)/user_perf_symbol_blobs.o
APP_IMAGE_EXTRA_OBJS :=
DEPS := $(foreach src,$(C_SRCS) $(S_SRCS),$(call dep_name,$(src)))
ifeq ($(USER_PERF_ENABLED),1)
APP_IMAGE_EXTRA_OBJS += $(UPERF_SYMBOLS_OBJ)
DEPS += $(UPERF_SYMBOLS_OBJ:.o=.d)
endif

.DEFAULT_GOAL := build

-include $(DEPS)

define register_c_rule
$(call obj_name,$(1)): $(1)
	@mkdir -p $$(dir $$@)
	$(CC) $(CPPFLAGS) -std=gnu23 $(CFLAGS) -MMD -MP -MF $$(patsubst %.o,%.d,$$@) -c $$< -o $$@
endef

define register_s_rule
$(call obj_name,$(1)): $(1)
	@mkdir -p $$(dir $$@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF $$(patsubst %.o,%.d,$$@) -c $$< -o $$@
endef

$(foreach src,$(C_SRCS),$(eval $(call register_c_rule,$(src))))
$(foreach src,$(S_SRCS),$(eval $(call register_s_rule,$(src))))

ifeq ($(USER_PERF_ENABLED),1)
$(UPERF_PRELINK_IMAGE): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) $(LDLIBS)

$(UPERF_SYMTAB_BIN): $(UPERF_PRELINK_IMAGE)
	@mkdir -p $(dir $@)
	@set -- $$($(READELF) -SW $< | awk '$$2 == ".symtab" { print $$5, $$6 }'); \
	dd if=$< of=$@ bs=1 skip=$$((16#$$1)) count=$$((16#$$2)) status=none

$(UPERF_STRTAB_BIN): $(UPERF_PRELINK_IMAGE)
	@mkdir -p $(dir $@)
	@set -- $$($(READELF) -SW $< | awk '$$2 == ".strtab" { print $$5, $$6 }'); \
	dd if=$< of=$@ bs=1 skip=$$((16#$$1)) count=$$((16#$$2)) status=none

$(UPERF_SYMBOLS_OBJ): $(UPERF_SYMBOL_BLOBS_SRC) $(UPERF_SYMTAB_BIN) $(UPERF_STRTAB_BIN)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -std=gnu23 $(CFLAGS) \
		-DUSER_PERF_SYMTAB_PATH=\"$(abspath $(UPERF_SYMTAB_BIN))\" \
		-DUSER_PERF_STRTAB_PATH=\"$(abspath $(UPERF_STRTAB_BIN))\" \
		-MMD -MP -MF $(patsubst %.o,%.d,$@) -c $(UPERF_SYMBOL_BLOBS_SRC) -o $@
endif

$(APP_IMAGE): $(OBJS) $(APP_IMAGE_EXTRA_OBJS)
	@mkdir -p $(dir $@)
	$(CC) -o $@ $(OBJS) $(APP_IMAGE_EXTRA_OBJS) $(LDFLAGS) $(LDLIBS)

.PHONY: build clean
build: $(APP_IMAGE)

clean:
	rm -rf $(BUILD_DIR)
