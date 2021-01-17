#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <gcrypt.h>

enum
{
	RSA_KEY_LEN_1024 = 1024,
	RSA_KEY_LEN_2048 = 2048,
	RSA_KEY_lEN_4096 = 4096,
};

enum
{
	RSA_KEY_FMT_CANON = GCRYSEXP_FMT_CANON,
	RSA_KEY_FMT_ADVANCED = GCRYSEXP_FMT_ADVANCED,
};

int gcrypt_gen_rsa_key(int key_len, int transient, int format,
		unsigned char **public, int *public_len,
		unsigned char **private, int *private_len)
{
	gcry_sexp_t key_spec = NULL,
				key = NULL,
				pubkey = NULL,
				seckey = NULL;

	char key_exp_str[256];
	int ret = 1;

	switch (key_len) {
		case RSA_KEY_LEN_1024:
		case RSA_KEY_LEN_2048:
		case RSA_KEY_lEN_4096:
			break;
		default:
			return 1;
	};

	switch (format) {
		case RSA_KEY_FMT_CANON:
		case RSA_KEY_FMT_ADVANCED:
			break;
		default:
			return 1;
	};

	if (sizeof(key_exp_str) == snprintf(key_exp_str, sizeof(key_exp_str),
				"(genkey (rsa %s(nbits 4:%d)))",
				transient ? "(flags transient-key) " : "",
				key_len))
	{
		fprintf(stderr, "bufoverflow\n");
		return 1;
	}

	if (gcry_sexp_new(&key_spec, key_exp_str, 0, 1)){
		fprintf(stderr, "new S-expression failed\n");
		return 1;
	}

	if (gcry_pk_genkey(&key, key_spec)) {
		fprintf(stderr, "genkey failed\n");
		goto error;

	}

	pubkey = gcry_sexp_find_token(key, "public-key", 0);
	if (pubkey == NULL) {
		fprintf(stderr, "no public-key found\n");
		goto error;
	}

	seckey = gcry_sexp_find_token(key, "private-key", 0);
	if (seckey == NULL) {
		fprintf(stderr, "no private-key found\n");
		goto error;
	}

	*public_len = gcry_sexp_sprint(pubkey, format, NULL, 0);
	*public = malloc(*public_len);
	if (*public == NULL) {
		fprintf(stderr, "malloc public key failed\n");
		goto error;
	}
	gcry_sexp_sprint(pubkey, format, *public, *public_len);

	*private_len = gcry_sexp_sprint(seckey, format, NULL, 0);
	*private = malloc(*private_len);
	if (*private == NULL) {
		fprintf(stderr, "malloc private key failed\n");
		goto error;
	}
	gcry_sexp_sprint(seckey, format, *private, *private_len);

	ret = 0;

error:
	if (key_spec)
		gcry_sexp_release(key_spec);
	if (key)
		gcry_sexp_release(key);
	if (pubkey)
		gcry_sexp_release(pubkey);
	if (seckey)
		gcry_sexp_release(seckey);

	return ret;
}

int write_file(const char *path, unsigned char *data, int len)
{
	int fd = open(path, O_CREAT|O_TRUNC|O_WRONLY, 0644);
	if (fd == -1) {
		fprintf(stderr, "open %s failed: %s\n", path, strerror(errno));
		return 1;
	}

	if (len != write(fd, data, len)) {
		fprintf(stderr, "write %s failed: %s\n", path, strerror(errno));
		return 1;
	}

	close(fd);
	return 0;
}

int main(int argc, char **argv)
{
	unsigned char *public;
	unsigned char *private;
	int public_len, private_len;
	int ret;
	char buf[1024];
	int fd;

	if (argc < 2) {
		fprintf(stderr, "usage: gen_rsa_key <prefix>\n");
		return 1;
	}

	ret = gcrypt_gen_rsa_key(RSA_KEY_LEN_2048, 1, RSA_KEY_FMT_CANON,
			&public, &public_len, &private, &private_len);
	if (ret)
		return ret;
	
	sprintf(buf, "%s.pub", argv[1]);
	write_file(buf, public, public_len);
	sprintf(buf, "%s.priv", argv[1]);
	write_file(buf, private, private_len);
	printf("success\n");
	return 0;
}

