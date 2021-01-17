app := toolchain
app-y += $(app)

define compile
app/$(app)/compile:;
endef

define install
app/$(app)/install:
	$(EE)
	$(H)cd $(TOOLCHAIN_SYSROOT_DIR)/lib && \
		$(Q)$(STRIP_INSTALL) $(subst ",,$(CONFIG_TOOLCHAIN_LIBS)) $(ROOTFS)/lib
	$(H)cd $(TOOLCHAIN_DEBUGROOT_DIR)/usr/bin && \
		$(Q)$(STRIP_INSTALL) $(subst ",,$(CONFIG_TOOLCHAIN_DEBUG_BINS)) $(ROOTFS)/usr/bin
endef

$(eval $(APP_BLD_XROUTER))

