app := libmnl
ver := $(CONFIG_LIBMNL_VER)
app-$(CONFIG_LIBMNL) += $(app)
binary := lib/libmnl.so*
$(eval $(APP_BLD_GPL))

