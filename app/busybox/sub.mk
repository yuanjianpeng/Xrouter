
app := busybox
ver := $(CONFIG_BUSYBOX_VER)

app-$(CONFIG_BUSYBOX) += $(app)

cflags := -Wno-format-truncation -Wno-format-overflow
host_cflags := -Wno-unused-result -Wno-format-security 
benv = CROSS_COMPILE=$(CROSS_COMPILE) $(MAKE) -C $(SDIR) O=$(ODIR)

define compile
app/$(app)/defconfig: app/$(app)/prepare
	[ -d $(ODIR) ] || mkdir -p $(ODIR)
	$(benv) defconfig
	cp $(ODIR)/.config $(MODEL_DIR)/$1.config

app/$(app)/menuconfig: app/$(app)/prepare
	[ -d $(ODIR) ] || mkdir -p $(ODIR)
	-cp $(MODEL_DIR)/$(app).config $(ODIR)/.config
	$(benv) menuconfig
	cp $(ODIR)/.config $(MODEL_DIR)/$(app).config

app/$(app)/compile: app/$(app)/prepare
	$(EE)
	$(H)[ -d $(ODIR) ] || mkdir -p $(ODIR)
	$(H)$(INSTALL) $(MODEL_DIR)/$(app).config $(ODIR)/.config 
	+$(HQ) CFLAGS="$(cflags)" $(benv) HOSTCFLAGS="$(host_cflags)"
endef

define install
app/$(app)/install:
	$(EE)
	+$(H)[ $(ROOTFS)/bin/busybox -nt $(ODIR)/busybox ] || $(Q) CFLAGS="$(cflags)" \
		$(benv) HOSTCFLAGS="$(host_cflags)" install CONFIG_PREFIX=$(ROOTFS)
endef

$(eval $(APP_BLD_GPL))

undefine cflags
undefine host_cflags

