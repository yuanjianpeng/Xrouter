
app := boa
ver := $(CONFIG_BOA_VER)
conf := --build=ix86 --host=$(host) CFLAGS="-Wno-unused-result -Wno-misleading-indentation"

app-$(CONFIG_BOA) += $(app)

define compile
app/$(app)/compile: app/$(app)/configure
	$(EE)
	+$(HQ)$(MAKE) -C $(ODIR)
endef

define install
app/$(app)/install:
	$(EE)
	$(HQ)$(STRIP_INSTALL) -t $(ROOTFS)/bin $(ODIR)/src/boa
endef

$(eval $(APP_BLD_GPL))

