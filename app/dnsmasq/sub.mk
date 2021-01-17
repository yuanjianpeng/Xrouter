
app := dnsmasq
ver := $(CONFIG_DNSMASQ_VER)

app-$(CONFIG_DNSMASQ) += $(app)

define compile
app/$(app)/compile: app/$(app)/prepare
	$(EE)
	+$(HQ)$(MAKE) -C $(SDIR) BUILDDIR=$(ODIR) CC=$(CC) all
endef

define install
app/$(app)/install:
	$(EE)
	$(HQ)$(STRIP_INSTALL) -t $(ROOTFS)/sbin $(ODIR)/dnsmasq
endef

$(eval $(APP_BLD_GPL))

