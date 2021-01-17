
app := libpcap
ver := $(CONFIG_LIBPCAP_VER)
conf := --enable-shared --disable-yydebug --with-pcap=linux --without-septel \
	--without-dag --without-libnl --without-snf --disable-usb \
	--disable-dbus --disable-bluetooth --enable-ipv6
mkenv := YFLAGS="-Wno-yacc -Wno-deprecated -Wno-other -Wno-conflicts-sr"
binary := lib/libpcap.so*

app-$(CONFIG_LIBPCAP) += $(app)

$(eval $(APP_BLD_GPL))

