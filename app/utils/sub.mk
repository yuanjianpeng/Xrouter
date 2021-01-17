app := utils
depends := libbase
app-$(CONFIG_UTILS) += $(app)

cflags := -I$(APP)
ldflags := -L../libbase -lbase

bin := stop_prog
stop_prog-obj := stop_prog.o

$(eval $(APP_BLD_XROUTER))

