app := base-files
app-y += $(app)

define compile
app/$(app)/compile:;
endef

define install
app/$(app)/install:
	$(EE)
	$(HQ)$(INSTALL) -D -v -b $(SDIR)/files $(ROOTFS)
	$(H)[ -d $(MODEL_DIR)/base-files ] && $(Q)$(INSTALL) -v -b $(MODEL_DIR)/base-files $(ROOTFS)
	$(H)cd $(ROOTFS) && $(INSTALL) -d dev run dev/pts proc sys tmp etc/rc.d
endef

$(eval $(APP_BLD_XROUTER))

