
TOOLCHAIN_CONFIG := $(MODEL_DIR)/toolchain.config
TOOLCHAIN_BUILD_DIR := $(OUTPUT)/toolchain
TOOLCHAIN_INSTALL_DIR := $(OPT)/$(CONFIG_TOOLCHAIN_NAME)
TOOLCHAIN_BIN_PATH := $(TOOLCHAIN_INSTALL_DIR)/bin
TOOLCHAIN_SYSROOT_DIR := $(TOOLCHAIN_INSTALL_DIR)/$(CONFIG_TOOLCHAIN_TARGET_TUPLE)/sysroot
TOOLCHAIN_DEBUGROOT_DIR := $(TOOLCHAIN_INSTALL_DIR)/$(CONFIG_TOOLCHAIN_TARGET_TUPLE)/debug-root

CROSS_COMPILE := $(TOOLCHAIN_BIN_PATH)/$(CONFIG_TOOLCHAIN_TARGET_TUPLE)-
CC := ${CROSS_COMPILE}gcc
AS := ${CROSS_COMPILE}as
CPP := ${CROSS_COMPILE}cpp
LD := ${CROSS_COMPILE}ld
STRIP := ${CROSS_COMPILE}strip
OBJDUMP := ${CROSS_COMPILE}objdump
OBJCOPY := ${CROSS_COMPILE}objcopy
READELF := ${CROSS_COMPILE}readelf
RANLIB := ${CROSS_COMPILE}ranlib
SIZE := ${CROSS_COMPILE}size
STIRNGS := ${CROSS_COMPILE}string
AR := ${CROSS_COMPILE}ar
NM := ${CROSS_COMPILE}nm
ADDR2LINE := ${CROSS_COMPILE}addr2line

ctng_src := $(TOOLCHAIN_BUILD_DIR)/$(CONFIG_CTNG_NAME)
ctng_tarball := $(CONFIG_TARBALL_DIR)/$(CONFIG_CTNG_NAME).tar.*

# crosstool-NG doesn't support out-of-tree build
# so we copy source to build dir, configure as local build
toolchain/prepare:
	$(E)
	$(H)[ -d $(ctng_src) ] ||  { \
		mkdir -p $(TOOLCHAIN_BUILD_DIR) ; \
		$(Q) ./host/install_source.sh $(ctng_src) $(ctng_tarball); }
	$(H)[ -f $(ctng_src)/Makefile ] || { cd $(ctng_src); $(Q) ./configure --enable-local; }
	$(H)[ -f $(ctng_src)/ct-ng ] || { cd $(ctng_src) && $(Q) MAKELEVEL=0 make; }

toolchain/menuconfig: toolchain/prepare
	[ ! -f $(TOOLCHAIN_CONFIG) ] || cp $(TOOLCHAIN_CONFIG) $(ctng_src)/.config 
	cd $(ctng_src) && ./ct-ng menuconfig
	cp $(ctng_src)/.config $(TOOLCHAIN_CONFIG)

toolchain/compile: toolchain/prepare kernel/prepare
	$(E)
	$(H)cp $(TOOLCHAIN_CONFIG) $(ctng_src)/.config 
	$(H)cd $(ctng_src) && \
		CTNG_TAR_DIR=$(realpath $(subst ",,$(CONFIG_TARBALL_DIR))) \
		CTNG_WORK_DIR=$(TOOLCHAIN_BUILD_DIR)/building_dir \
		CTNG_PREFIX_DIR=$(TOOLCHAIN_INSTALL_DIR) \
		CTNG_KERNEL_SRC=$(KERNEL_SRC_DIR) \
		./ct-ng build $(Q)

toolchain/clean:
	rm -fr $(ctng_src) $(TOOLCHAIN_INSTALL_DIR)

toolchain: toolchain/compile; 

toolchain/install-dist:
	tar xf $(CONFIG_TARBALL_DIR)/$(CONFIG_TOOLCHAIN_NAME).tar.* -C $(OPT_DIR)
	
.PHONY: toolchain toolchain/prepare toolchain/compile toolchain/clean toolchain/menuconfig \
		toolchain/download toolchain/install-dist

