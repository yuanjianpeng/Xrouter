
app := libnl
ver := $(CONFIG_LIBNL_VER)
conf := --disable-static --disable-cli --disable-pthreads --disable-debug
binary := lib/libnl*.so*
benv := PKG_CONFIG="" \
	CFLAGS="-Wno-stringop-truncation -Wno-unused-function -Wno-misleading-indentation"
mkenv := YFLAGS="-Wno-yacc -Wno-deprecated -Wno-other"

app-$(CONFIG_LIBNL) += $(app)

$(eval $(APP_BLD_GPL))

