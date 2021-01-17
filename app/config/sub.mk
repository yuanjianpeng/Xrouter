
app := config
depends := libbase log
app-$(CONFIG_CONFIG) += $(app)

cflags := -I$(APP)

lib := libconfig
libconfig-obj := config.o
libconfig-ldflags := -L../libbase -lbase -L../log -llog

bin := cfg
cfg-obj := config_cmd.o
cfg-ldflags := $(libconfig-ldflags) -L. -lconfig

$(eval $(APP_BLD_XROUTER))

