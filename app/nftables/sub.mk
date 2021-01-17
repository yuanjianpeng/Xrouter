
app := nftables
ver := $(CONFIG_NFTABLES_VER)
depends := libmnl libnftnl 
conf := LIBMNL_CFLAGS=-I$(call I,libmnl) \
	LIBMNL_LIBS="-Wl$(C)-L$(call L,libmnl)$(C)-lmnl" \
	LIBNFTNL_CFLAGS=-I$(call I,libnftnl) \
	LIBNFTNL_LIBS="-Wl$(C)-L$(call L,libnftnl)$(C)-lnftnl" \
	CFLAGS="-Wno-cast-align -Wno-sign-compare" \
	LDFLAGS="-Wl$(C)-rpath-link=$(call L,libmnl)$(C)-rpath-link=$(call L,libnftnl)" \
	--disable-man-doc --with-mini-gmp --without-cli
binary := lib/libnftables.so* sbin/nft

app-$(CONFIG_NFTABLES) += $(app)

$(eval $(APP_BLD_GPL))

