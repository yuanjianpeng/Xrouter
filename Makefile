# Yuan Jianpeng
# 2019/12/15

TOP := $(CURDIR)

all: host kernel app target;

include common.mk

menuconfig: host/kconfig
	KCONFIG_CONFIG=$(MODEL_CONFIG) $(HOST_OUTPUT)/kconfig/mconf Config.in

# uncompress source code tarball and apply patches
prepare: kernel/prepare app/prepare;

# clean source code
prepare/clean:
	rm -fr $(SOURCE)

# download source code tarball
download: kernel/download app/download;

# clean build
clean:
	rm -fr $(OUTPUT)

dist-clean:
	rm -fr $(STAGING)

.PHONY: all menuconfig prepare prepare/clean download clean dist-clean

.NOTPARALLEL:

