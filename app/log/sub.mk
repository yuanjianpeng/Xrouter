app := log
depends := libbase
app-$(CONFIG_LOG) += $(app)

lib := liblog
liblog-obj := log.o
liblog-cflags := -I$(APP)
liblog-ldflags := -L../libbase -lbase

bin := log
log-obj := log_cmd.o
log-ldflags := $(liblog-ldflags) -L. -llog

$(eval $(APP_BLD_XROUTER))

