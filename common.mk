
MODEL := $(if $(MODEL),$(MODEL)$(file >.model,$(MODEL)),$(file <.model))
STAGING := $(or $(if $(O),$(O)$(file >.staging,$(O)),$(file <.staging)),$(TOP)/staging)
override undefine O

OPT := /opt
HOST := $(TOP)/host
TOOLCHAIN := $(TOP)/toolchain
KERNEL := $(TOP)/kernel
APP := $(TOP)/app
TARGET := $(TOP)/targets
MODEL_DIR := $(TARGET)/$(MODEL)
MODEL_CONFIG := $(MODEL_DIR)/model.config
SOURCE := $(STAGING)/source
OUTPUT := $(STAGING)/$(MODEL)
ROOTFS := $(OUTPUT)/rootfs

# CONFIG_XXX
include $(MODEL_CONFIG)

# 
# make				(print only target name)
# make v=1			(print only recipes)
# make v=<other>	(print everything)
#
empty :=
ifeq ($v,)
E = @echo "$@" 
EE = @echo "$$@" 
Q := >/dev/null $(empty)
H := @
HQ := $(H)$(Q)
else ifeq ($v,1)
Q := >/dev/null $(empty)
HQ := $(Q)
else
E = @echo "building $@ ..." 
EE = @echo "building $$@ ..." 
endif

# variables define start

# defualt CFLAGS & LDFLAGS
TARGET_CFLAGS := -g -O2 -Wall -Werror -Werror=format-security -Werror=implicit-function-declaration
TARGET_LDFLAGS := -Wl,-z,now

# variables define end

include $(TOOLCHAIN)/sub.mk
include $(HOST)/sub.mk
include $(KERNEL)/sub.mk
include $(APP)/sub.mk
include $(TARGET)/sub.mk


# smart dependiencies

