app := libnftnl
ver := $(CONFIG_LIBNFTNL_VER)
depends := libmnl
conf := LIBMNL_CFLAGS=-I$(call I,libmnl) LIBMNL_LIBS="-Wl$(C)-L$(call L,libmnl)$(C)-lmnl"
binary := lib/libnftnl.so*

app-$(CONFIG_LIBNFTNL) += $(app)

$(eval $(APP_BLD_GPL))

