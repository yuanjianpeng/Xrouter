
app := webcgi
depends := libbase log config switch
app-$(CONFIG_WEBCGI) += $(app)

bin := webcgi
webcgi-obj := webcgi.o http.o cgi/network.o cgi/system.o

cflags := -I$(APP)
ldflags := -L../libbase -lbase -L../config -lconfig -L../switch -lswitch -L../log -llog

$(eval $(APP_BLD_XROUTER))

