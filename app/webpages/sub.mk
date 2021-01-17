app := webpages
app-y += $(app)

define compile
app/$(app)/compile:;
endef

define install
app/$(app)/install:
	$(EE)
	$(HQ)$(INSTALL) -D -v -b $(SDIR)/files $(ROOTFS)/web
endef

$(eval $(APP_BLD_XROUTER))


