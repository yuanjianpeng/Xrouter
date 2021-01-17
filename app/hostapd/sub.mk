
app := hostapd
ver := $(CONFIG_HOSTAPD_VER)
depends := libnl
benv := CC=$(CC) LIBNL_INC="$(call I,libnl)/libnl3" LIBS="-L$(call L,libnl)"

app-$(CONFIG_HOSTAPD) += $(app)

define compile
app/$(app)/compile: $(call DEPS,$(depends)) app/$(app)/prepare
	$(EE)
	$(H)[ -f $(ODIR)/$(app)/.config ] || { mkdir -p $(ODIR)/$(app); \
		cp $(APP)/$(app)/$(app).config $(ODIR)/$(app)/.config ; }
	+$(HQ) $(benv) $(MAKE) -C $(SDIR)/$(app) O=$(ODIR)/$(app)/
endef

define install
app/$(app)/install:
	$(EE)
	$(HQ)$(STRIP_INSTALL) -t $(ROOTFS)/usr/sbin $(ODIR)/$(app)/hostapd \
		$(ODIR)/$(app)/hostapd_cli
endef

$(eval $(APP_BLD_GPL))

