
app := ath10k-ct
ver := $(CONFIG_ATH10K_CT_VER)
depends := mac80211

app-$(CONFIG_ATH10K_CT) += $(app)

CT_MAKEDEFS := CONFIG_ATH10K=m CONFIG_ATH10K_PCI=m CONFIG_ATH10K_CE=y \
	CONFIG_ATH10K_AHB=m CONFIG_ATH10K_DEBUGFS=y CONFIG_MAC80211_DEBUGFS=y CONFIG_ATH10K_LEDS=y 

CT_INCLUDES := \
	-I$(APP_BUILD)/backports/usr/include/mac80211-backport/uapi \
	-I$(APP_BUILD)/backports/usr/include/mac80211-backport \
	-I$(APP_BUILD)/backports/usr/include/mac80211/uapi \
	-I$(APP_BUILD)/backports/usr/include/mac80211 \
	-include backport/autoconf.h \
	-include backport/backport.h \
	-DCONFIG_MAC80211_MESH  \
	-DCONFIG_ATH10K_AHB  \
	-DSTANDALONE_CT \
	-DCONFIG_MAC80211_DEBUGFS \
	-DCONFIG_ATH10K_DEBUGFS \
	-DCONFIG_ATH10K_DFS_CERTIFIED \
	-DCONFIG_ATH10K_LEDS

ctver := $(CONFIG_ATH10K_CT_CTVER)

define compile
app/$(app)/compile: $(call DEPS,$(depends)) app/$(app)/prepare
	$(EE)
	$(H)[ -d $(ODIR)/ath10k-$(ctver) ] || mkdir -p $(ODIR)/ath10k-$(ctver)
	+$(HQ)$(KERNEL_ENV) $$(CT_MAKEDEFS) NOSTDINC_FLAGS="$$(CT_INCLUDES)" \
		M=$(ODIR)/ath10k-$(ctver) KBUILD_EXTMOD_SRC=$(SDIR)/ath10k-$(ctver) \
		KBUILD_EXTRA_SYMBOLS=$(APP_BUILD)/backports/Module.symvers modules
endef

define install
app/$(app)/install:
	$(EE)
	+$(HQ)$(KERNEL_ENV) M="$(ODIR)/ath10k-$(ctver)" \
		KBUILD_EXTMOD_SRC=$(SDIR)/ath10k-$(ctver) INSTALL_MOD_STRIP=1 \
		INSTALL_MOD_PATH=$(ROOTFS) modules_install
endef

$(eval $(APP_BLD_GPL))

