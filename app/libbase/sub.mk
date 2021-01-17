
app := libbase
app-$(CONFIG_LIBBASE) += $(app)

lib := libbase
libbase-obj := error.o base.o system.o filesystem.o mtd.o netlink.o net.o

$(eval $(APP_BLD_XROUTER))

