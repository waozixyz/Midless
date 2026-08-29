# Midless - voxel client running on the kryon runtime.
# Desktop local-world build; the server and web transports are out of scope
# for now, so their sources are not compiled.

.DEFAULT_GOAL := run

APP_NAME := katalis
KRYON_DIR := $(if $(wildcard vendor/kryon/mk/common.mk),vendor/kryon,../kryon)
KRYON_MAKE_DIR := $(KRYON_DIR)/mk/

KRYON_WITH_PHYSICS := 0
KRYON_NATIVE_SUPPORT_FLAGS := -DSUPPORT_MODULE_RAUDIO=0
KRYON_RAYLIB_MODULE_AUDIO := FALSE
export KRYON_COMPAT_AUDIO := 0

include $(KRYON_MAKE_DIR)common.mk
include $(KRYON_MAKE_DIR)raylib.mk
include $(KRYON_MAKE_DIR)vendor.mk

KRYON_INCLUDE += $(KRYON_PHYSICS_CPPFLAGS)
KRYON_SRCS := $(filter-out $(KRYON_PHYSICS_SRCS),$(KRYON_SRCS))
KRYON_SRCS := $(filter-out $(KRYON_DIR)/src/ksync/%.c,$(KRYON_SRCS))
KRYON_SRCS := $(filter-out $(KRYON_DIR)/src/file_dialog/file_dialog.c,$(KRYON_SRCS))
KRYON_SRCS := $(filter-out $(KRYON_DIR)/src/runtime_assets/%.c,$(KRYON_SRCS))
KRYON_MARKDOWN_DEPS :=
KRYON_MARKDOWN_CFLAGS :=
KRYON_MARKDOWN_LDLIBS :=
FONT_FILES :=

# Client sources; the enet and websocket transports stay out of the build
# (singleplayer keeps working because world edits already apply locally).
# common.mk already appended the generated embedded-assets source to SRC;
# keep it by appending rather than assigning.
SRC += $(filter-out client/src/networking/client.c client/src/networking/clientws.c,\
	$(shell find client/src -type f -name '*.c' | LC_ALL=C sort))

CFLAGS += -DOS_LINUX
CFLAGS += -Iclient -Iclient/src -Iclient/src/chunk -Iclient/src/block \
	-Iclient/src/entity -Iclient/src/gui -Iclient/src/networking -Ilibs
CFLAGS += -Wno-unused-function -Wno-unused-parameter -Wno-sign-compare -Wno-format-truncation

include $(KRYON_MAKE_DIR)native.mk
include $(KRYON_MAKE_DIR)clean.mk
