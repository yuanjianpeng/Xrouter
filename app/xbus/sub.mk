app := xbus
depends := libbase
app-$(CONFIG_XBUS) += $(app)

cflags := -I$(APP)

lib := libxbus
libxbus-obj := xbus.o
libxbus-ldflags := -L../libbase -lbase

bin := xbus
xbus-obj := xbus_cmd.o
xbus-ldflags := $(libxbus-ldflags) -L. -lxbus

$(eval $(APP_BLD_XROUTER))

