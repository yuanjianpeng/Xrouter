
app := wpa_supplicant
ver := $(CONFIG_WPA_SUPPLICANT_VER)
depends := libnl
benv := CC=$(CC) LIBNL_INC="$(call I,libnl)/libnl3" LIBS="-L$(call L,libnl)"

bins := wpa_supplicant wpa_cli wpa_passphrase

app-$(CONFIG_WPA_SUPPLICANT) += $(app)

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
	$(HQ)$(STRIP_INSTALL) -t $(ROOTFS)/usr/sbin $(ODIR)/$(app)/wpa_supplicant \
		$(ODIR)/$(app)/wpa_cli $(ODIR)/$(app)/wpa_passphrase
endef

$(eval $(call APP_BLD_GPL))

