
app := libgcrypt
ver := $(CONFIG_LIBGCRYPT_VER)
depends := libgpg-error
conf := --disable-asm --disable-doc \
	--with-libgpg-error-prefix=$(APP_BUILD)/libgpg-error/ \
	--enable-digests="md5 rmd160 sha1 sha256 sha512 blake2" \
	--enable-ciphers="arcfour des aes cast5 chacha20" \
	--enable-pubkey-ciphers="rsa dsa ecc" \
	CFLAGS="-Wno-switch"
binary := lib/libgcrypt.so*
app-$(CONFIG_LIBGCRYPT) += $(app)

$(eval $(APP_BLD_GPL))

