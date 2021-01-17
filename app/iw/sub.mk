app := iw
ver := $(CONFIG_IW_VER)
depends := libnl
app-$(CONFIG_IW) += $(app)

define compile
app/$(app)/compile: $(call DEPS,$(depends)) app/$(app)/prepare
	$(EE)
	$(H)[ -d $(ODIR) ] || mkdir -p $(ODIR)
	+$(H)cd $(ODIR) ; $(Q) CC=$(CC) CFLAGS="-DCONFIG_LIBNL30 -I$(call I,libnl)/libnl3" \
		LDFLAGS="-L$(call L,libnl) -lnl-3 -lnl-genl-3" \
		VPATH="$(SDIR)" $(MAKE) -f $(SDIR)/Makefile NO_PKG_CONFIG=y 
endef

define install
app/$(app)/install:
	$(EE)
	$(HQ)$(STRIP_INSTALL) -t $(ROOTFS)/bin $(ODIR)/iw
endef

$(eval $(APP_BLD_GPL))

