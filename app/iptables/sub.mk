
app := iptables
ver := $(CONFIG_IPTABLES_VER)
conf := --disable-nftables
benv := CFLAGS="-Wno-pointer-to-int-cast"
binary := sbin/* lib/lib*.so* lib/xtables/*

app-$(CONFIG_IPTABLES) += $(app)

$(eval $(APP_BLD_GPL))

