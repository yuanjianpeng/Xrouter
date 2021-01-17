
app := switch
depends := libbase log
app-$(CONFIG_SWITCH) += $(app)

cflags := -I$(APP)

lib := libswitch
libswitch-obj := libswitch.o
libswitch-ldflags := -L../libbase -lbase

bin := sw
sw-obj := sw.o
sw-ldflags := -L../libbase -lbase -L../log -llog -L. -lswitch

kmodule := switch.ko

$(eval $(APP_BLD_XROUTER))

