
app := tcpdump
ver := $(CONFIG_TCPDUMP_VER)
depends := libpcap
conf := --disable-smb
binary := sbin/tcpdump

app-$(CONFIG_TCPDUMP) += $(app)

$(eval $(APP_BLD_GPL))

