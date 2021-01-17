
HOST_OUTPUT := $(STAGING)/host
INSTALL_SRC := $(HOST)/install_source.sh
INSTALL := $(HOST_OUTPUT)/install
STRIP_INSTALL := STRIPPROG=$(STRIP) STRIPOPT=-s $(INSTALL) -v -D -s

host: host/kconfig host/install host/mkfw;

host/kconfig:
	$(E)
	$(H)[ -d $(HOST_OUTPUT) ] || mkdir -p $(HOST_OUTPUT)
	+$(HQ)$(MAKE) -C $(HOST)/kconfig O=$(HOST_OUTPUT)/kconfig/

host/install:
	$(E)
	$(H)[ -d $(HOST_OUTPUT) ] || mkdir -p $(HOST_OUTPUT)
	+$(HQ)$(MAKE) -C "$(HOST_OUTPUT)" -f $(HOST)/rules.mk src="$(HOST)/install" \
		bin=install install-obj="install.o mkdir.o"

host/mkfw:
	$(E)
	$(H)[ -d $(HOST_OUTPUT) ] || mkdir -p $(HOST_OUTPUT)
	+$(HQ)$(MAKE) -C "$(HOST_OUTPUT)" -f $(HOST)/rules.mk src="$(HOST)/mkfw" \
		bin="gen_rsa_key mkfw" ldflags="-lgcrypt" cflags="-I$(APP)/upgrade/"

host/clean:
	rm -fr $(HOST_OUTPUT)

.PHONY: host/kconfig host/clean host/install host/mkfw

