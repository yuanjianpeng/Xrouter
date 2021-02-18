
app := libgpg-error
ver := $(CONFIG_LIBGPGERROR_VER)
conf := --disable-languages --disable-doc --disable-tests --disable-nls
binary := lib/libgpg-error.so*
app-$(CONFIG_LIBGPGERROR) += $(app)

$(eval $(APP_BLD_GPL))

