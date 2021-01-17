# Yuan Jianpeng
# 2019/12/15

host := $(CONFIG_TOOLCHAIN_TARGET_TUPLE)
APP_BUILD := $(OUTPUT)/app

C := ,
I = $(APP_BUILD)/$1/include
L = $(APP_BUILD)/$1/lib
DEPS = $(patsubst %,app/%/compile,$1)

define APP_PREP
app/$1/prepare:
	$(EE)
	$(H)[ -d $(SOURCE)/$2-$3 ] || { \
		mkdir -p $(SOURCE); \
		$(Q) ./host/install_source.sh $(SOURCE)/$2-$3 $(CONFIG_TARBALL_DIR)/$2-$3.tar.* \
			$(APP)/$1/patches-$3 ; \
	}
endef

define check_var
ifneq ($(origin $1),undefined)
$$(error $1 is reserved for app, forbbiden use)
endif
endef

VAR_gpl := app ver depends benv conf configure mkenv compile install binary
$(foreach var,$(VAR_gpl),$(eval $(call check_var,$(var))))


# for famous GPL project, which use Autotools
#
# if toolchain doesn't provide some tools, e.g. arm-linux-gnueabihf-link
# configure will try to find it in $PATH, if it find one, configure will
# WARNING: using cross tools not prefixed with host triplet.
# so we set ac_tool_warned=yes to disable this anoying warning message.
#
# we can't build like this:
#	./configure --prefix=/
#	make install DESTDIR=$(STAGING)
# when libtool install, its relink command add -L //lib,
# that make cross compiler find library in host library directory, we get
# error: //lib/libgcc_s.so.1: file not recognized: file format not recognized
# 
# The solution is 
#	./configure --prefix=$(STAGING)
#	make install
# we set lt_cv_sys_lib_dlsearch_path_spec to $(STAING) to avoid libtool
# embed $(STAGING)/lib to library's run path,
#
define APP_BLD_GPL

$(eval SDIR := $(SOURCE)/$(app)-$(ver))
$(eval ODIR := $(APP_BUILD)/$(app)-$(ver))
$(eval STAGING := $(ODIR)/staging)
$(eval BENV := $(benv) ac_tool_warned=yes CC=$(CC) AR=$(AR) INSTALL=$(INSTALL))

$(eval $(call APP_PREP,$(app),$(app),$(ver)))

ifeq ($(origin configure),undefined)
app/$(app)/configure: $(call DEPS,$(depends)) app/$(app)/prepare
	$(EE)
	$(HQ)[ -d $(ODIR) ] || mkdir -p $(ODIR)
	$(H)cd $(ODIR) && { [ -f Makefile ] || \
		$(Q)$(BENV) $(SDIR)/configure --host=$(host) --prefix=$(STAGING) \
		lt_cv_sys_lib_dlsearch_path_spec=$(STAGING)/lib $(conf); }
else
$(configure)
endif

ifeq ($(origin compile),undefined)
app/$(app)/compile: $(call DEPS,$(depends)) app/$(app)/configure
	$(EE)
	$(H)[ -h $(APP_BUILD)/$(app) ] || $(Q) ln -s $(app)-$(ver)/staging $(APP_BUILD)/$(app)
	+$(HQ) $(MAKE) -C $(ODIR) $(mkenv)
	+$(HQ)$(MAKE) -C $(ODIR) install
else
$(compile)
endif

ifeq ($(origin install),undefined)
app/$(app)/install:
	$(EE)
	$(H)cd $(STAGING) && $(Q)$(STRIP_INSTALL) -k -t $(ROOTFS) $(binary)
else
$(install)
endif

app/$(app)/clean:
	rm -fr $(ODIR) $(APP_BUILD)/$(app)

app/$(app)/clean-src: 
	rm -fr $(ODIR) $(APP_BUILD)/$(app) $(SDIR)

app/$(app): app/$(app)/compile app/$(app)/install;

# clear per-app variables so they don't affect next GPL app
__dummy := $(foreach var,$(VAR_gpl),$(eval undefine $(var)))

endef

ENV_common = CC=$(CC) ARCH=$(CONFIG_ARCH) CROSS_COMPILE='$(CROSS_COMPILE)' \
	CFLAGS='$(TARGET_CFLAGS)' LDFLAGS='$(TARGET_LDFLAGS)' \
	KERNEL_BUILD='$(KERNEL_BUILD)' 

define ENV_APP
$1-obj="$($1-obj)" \
$1-cflags="$($1-cflags)" \
$1-ldflags="$($1-ldflags)"
endef

VAR_xrouter := app depends lib bin cflags ldflags kmodule kmodule-env install compile
$(foreach var,$(VAR_xrouter),$(eval $(call check_var,$(var))))

# for xrouter apps
define APP_BLD_XROUTER

$(eval SDIR := $(APP)/$(app))
$(eval ODIR := $(APP_BUILD)/$(app))
$(eval ENV_app := lib='$(lib)' bin='$(bin)' cflags='$(cflags)' ldflags='$(ldflags)')
$(eval ENV_app += kmodule='$(kmodule)' kmodule_env='$(kmodule-env)')
$(eval ENV_app += $(foreach bin,$(lib) $(bin),$(call ENV_APP,$(bin))))

ifeq ($(origin compile),undefined)
app/$(app)/compile: $(call DEPS,$(depends))
	$(EE)
	$(H)[ -d $(ODIR) ] || mkdir -p $(ODIR)
	+$(HQ) $(ENV_common) $(MAKE) $(ENV_app) -C "$(ODIR)" src="$(SDIR)" -f $(HOST)/rules.mk
else
$(compile)
endif

ifeq ($(origin install),undefined)
app/$(app)/install:
	$(EE)
ifneq ($(lib),)
	$(HQ)$(STRIP_INSTALL) -t $(ROOTFS)/usr/lib $(lib:%=$(ODIR)/%.so)
endif
ifneq ($(bin),)
	$(HQ)$(STRIP_INSTALL) -t $(ROOTFS)/usr/bin $(bin:%=$(ODIR)/%)
endif
ifneq ($(kmodule),)
	+$(HQ)$(KERNEL_ENV) M=$(ODIR) KBUILD_EXTMOD_SRC=$(SDIR) INSTALL_MOD_STRIP=1 INSTALL_MOD_PATH=$(ROOTFS) modules_install
endif
else
$(install)
endif

app/$(app)/clean:
	rm -fr $(ODIR)

app/$(app): app/$(app)/compile app/$(app)/install;

# clear per-app variables so they don't affect next app
__dummy := $(foreach var,$(foreach bin,$(lib) $(bin),$(bin)-obj $(bin)-cflags $(bin)-ldflags) \
	$(VAR_xrouter),$(eval undefine $(var)))

endef

app-y :=
include $(APP)/*/sub.mk

app/clean:
	rm -fr $(APP_BUILD)

app: $(patsubst %,app/%,$(app-y));
app/compile: $(patsubst %,app/%/compile,$(app-y));
app/install: $(patsubst %,app/%/install,$(app-y));

.PHONY: app/clean app/compile app/install

