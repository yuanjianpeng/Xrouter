
KERNEL_CONFIG := $(MODEL_DIR)/kernel.config
KERNEL_NAME := linux-$(CONFIG_KERNEL_VER)
KERNEL_SRC := $(SOURCE)/$(KERNEL_NAME)
KERNEL_BUILD := $(OUTPUT)/$(KERNEL_NAME)
KERNEL_TARBALL := $(CONFIG_TARBALL_DIR)/$(KERNEL_NAME).tar.xz
KERNEL_PATCH := $(KERNEL)/patches-$(CONFIG_KERNEL_VER)
KERNEL_ENV := ARCH=$(CONFIG_ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
				$(MAKE) -C $(KERNEL_SRC) O=$(KERNEL_BUILD)

kernel/prepare:
	$(E)
	$(H)[ -d $(KERNEL_BUILD) ] || mkdir -p $(KERNEL_BUILD)
	$(H)[ -d "$(KERNEL_SRC)" ] || { \
		mkdir -p $(SOURCE); \
		$(Q) ./host/install_source.sh $(KERNEL_SRC) $(KERNEL_TARBALL) $(KERNEL_PATCH); }

kernel/menuconfig: kernel/prepare
	cp $(KERNEL_CONFIG) $(KERNEL_BUILD)/.config
	$(KERNEL_ENV) menuconfig
	cp $(KERNEL_BUILD)/.config $(KERNEL_CONFIG)

kernel/compile: kernel/prepare
	$(E)
	$(H)cp $(KERNEL_CONFIG) $(KERNEL_BUILD)/.config
	+$(HQ)$(KERNEL_ENV)

# install kernel modules to rootfs
kernel/install:
	$(E)
	+$(HQ)$(KERNEL_ENV) INSTALL_MOD_STRIP=1 INSTALL_MOD_PATH=$(ROOTFS) modules_install

kernel/clean:
	rm -fr $(KERNEL_BUILD)

kernel/clean-src:
	rm -fr $(KERNEL_BUILD) $(KERNEL_SRC)

kernel: kernel/compile kernel/install;

