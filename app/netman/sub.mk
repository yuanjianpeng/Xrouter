app := netman
depends := libbase log config xbus
app-$(CONFIG_NETMAN) += $(app)

cflags := -I$(APP)
ldflags := -L../libbase -lbase -L../log -llog -L../config -lconfig -L../xbus -lxbus

bin := netman
netman-obj := config.o dhcpc.o netman.o proto.o wifi.o

$(eval $(APP_BLD_XROUTER))

