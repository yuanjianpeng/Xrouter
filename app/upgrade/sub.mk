app := upgrade
depends := libbase
app-$(CONFIG_UPGRADE) += $(app)
bin := upfw
cflags := -I../libgpg-error/include -I../libgcrypt/include
$(eval $(APP_BLD_XROUTER))

