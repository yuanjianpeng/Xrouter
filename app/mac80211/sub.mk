
app := mac80211
ver := $(CONFIG_MAC80211_VER)

app-$(CONFIG_MAC80211) += $(app)

define APP_BLD_BACKPORTS

$(eval SDIR := $(SOURCE)/backports-$(ver))
$(eval ODIR := $(APP_BUILD)/backports-$(ver))
$(eval BENV := CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(CONFIG_ARCH) BACKPORT_BUILD_DIR=$(ODIR) \
	$(MAKE) -C $(SDIR) KLIB_BUILD=$(KERNEL_BUILD) KLIB=$(ROOTFS))

$(eval $(call APP_PREP,$(app),backports,$(ver)))

app/$(app)/menuconfig:
	$(EE)
	cp $(MODEL_DIR)/mac80211.config $(SDIR)/.config
	$(H)$(BENV) menuconfig
	cp $(SDIR)/.config $(MODEL_DIR)/mac80211.config

app/$(app)/compile: app/$(app)/prepare
	$(EE)
	$(H)cp $(MODEL_DIR)/mac80211.config $(SDIR)/.config
	$(H)[ -d $(ODIR) ] || mkdir -p $(ODIR)
	+$(HQ)$(BENV) modules
	$(H)[ -h $(APP_BUILD)/backports ] || $(Q) ln -s backports-$(ver)/staging \
		$(APP_BUILD)/backports
	$(H)[ -d $(ODIR)/staging/usr/include ] || { \
		set -e ; \
		mkdir -p \
			$(ODIR)/staging/usr/include/mac80211 \
			$(ODIR)/staging/usr/include/mac80211-backport \
			$(ODIR)/staging/usr/include/mac80211/ath \
			$(ODIR)/staging/usr/include/net/mac80211 ; \
		cp -r $(SDIR)/net/mac80211/*.h $(SDIR)/include/* $(ODIR)/staging/usr/include/mac80211/ ; \
		cp -r $(SDIR)/backport-include/* $(ODIR)/staging/usr/include/mac80211-backport/ ; \
		cp $(SDIR)/net/mac80211/rate.h $(ODIR)/staging/usr/include/net/mac80211/ ; \
		cp $(SDIR)/drivers/net/wireless/ath/*.h $(ODIR)/staging/usr/include/mac80211/ath/ ; \
		rm -f $(ODIR)/staging/usr/include/mac80211-backport/linux/module.h ; \
	}
	$(H)[ -h $(ODIR)/staging/Module.symvers ] || ln -s ../Module.symvers \
		$(ODIR)/staging/Module.symvers

app/$(app)/install:
	$(EE)
	+$(HQ)$(KERNEL_ENV) M="$(ODIR)" KBUILD_EXTMOD_SRC=$(SDIR) INSTALL_MOD_STRIP=1 \
		INSTALL_MOD_PATH=$(ROOTFS) modules_install

app/$(app)/clean:
	rm -fr $(ODIR) $(APP_BUILD)/backports

app/$(app)/clean-src:
	rm -fr $(SDIR)

app/$(app): app/$(app)/compile app/$(app)/install;

endef

$(eval $(call APP_BLD_BACKPORTS))

