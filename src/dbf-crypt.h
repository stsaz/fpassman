/** fpassman: AES encryption
2018,2022 Simon Zolin
*/

#include <FFOS/random.h>
#include <aes/aes-ff.h>
#include <sha/sha256.h>
#include <sha1/sha1.h>

enum {
	AESBUFSIZE = 16*1024,
};

static void set_iv_v11(byte iv[16], const byte *key)
{
	byte sha1sum[SHA1_LENGTH];
	sha1_ctx sha1;
	sha1_init(&sha1);
	sha1_update(&sha1, key, FPM_DB_KEYLEN);
	sha1_fin(&sha1, sha1sum);
	memcpy(iv, sha1sum, 16);
}

static void random_iv(byte iv[16])
{
	fftime now;
	fftime_now(&now);
	ffrand_seed(now.sec);
	uint *p = (uint*)iv;
	p[0] = ffrand_get();
	p[1] = ffrand_get();
	p[2] = ffrand_get();
	p[3] = ffrand_get();
}

static int encrypt(fpm_db *db, ffstr *src, ffstr *dststr, const byte *key)
{
	aes_ctx en = {};
	byte iv[16];
	byte *dst;
	ffstr buf = {};
	ffvec d = {};
	char tmp[1024];
	int r = 1;

	if (NULL == ffstr_alloc(&buf, AESBUFSIZE))
		goto done;
	dst = (void*)buf.ptr;

	if (0 != aes_encrypt_init(&en, key, FPM_DB_KEYLEN, AES_CBC))
		goto done;

	random_iv(iv);
	ffvec_add(&d, iv, 16, 1);

	while (src->len != 0) {
		size_t len2 = ffmin(src->len, 1024);

		if (len2 < 1024) {
			size_t sz;
			// length must be a multiple of 16
			memcpy(tmp, src->ptr, len2);
			sz = len2 + (16 - (len2 % 16));
			memset(tmp + len2, ' ', sz - len2);
			len2 = sz;
			src->ptr = tmp;
			src->len = len2;
		}

		if (0 != aes_encrypt_chunk(&en, (byte*)src->ptr, dst, len2, iv))
			goto done;
		ffstr_shift(src, len2);
		if (NULL == ffvec_grow(&d, len2, 1))
			goto done;
		ffvec_add(&d, buf.ptr, len2, 1);
	}

	ffstr_set(dststr, d.ptr, d.len);
	r = 0;

done:
	aes_encrypt_destroy(&en);
	ffstr_free(&buf);
	if (r != 0)
		ffvec_free(&d);
	return r;
}

struct dbf_crypt {
	aes_ctx de;
	byte iv[16];
	ffstr decrypted;
	uint need_iv :1;
};

static int decrypt_init(uint flags, struct dbf_crypt *x, const byte *key)
{
	if (0 != aes_decrypt_init(&x->de, key, FPM_DB_KEYLEN, AES_CBC)) {
		errlog("aes_decrypt_init()", 0);
		return -1;
	}

	if (flags & FPM_OPEN_COMPAT_V11)
		set_iv_v11(x->iv, key);
	else
		x->need_iv = 1;

	if (NULL == ffstr_alloc(&x->decrypted, AESBUFSIZE))
		return -1;
	return 0;
}

static void decrypt_close(struct dbf_crypt *x)
{
	ffmem_zero(x->decrypted.ptr, x->decrypted.len);
	ffstr_free(&x->decrypted);
	aes_decrypt_destroy(&x->de);
}

static int decrypt(struct dbf_crypt *x, ffstr in, ffstr *buf)
{
	if (x->need_iv) {
		x->need_iv = 0;
		if (in.len < 16) {
			errlog("set IV: not enough input data");
			return -1;
		}
		ffmem_copy(x->iv, in.ptr, 16);
		ffstr_shift(&in, 16);
	}

	byte *dst = (void*)x->decrypted.ptr;
	while (in.len != 0) {
		size_t len2 = ffmin(in.len, 1024);
		if (0 != aes_decrypt_chunk(&x->de, (byte*)in.ptr, dst, len2, x->iv)) {
			errlog("aes_decrypt_chunk()", 0);
			return -1;
		}
		ffstr_shift(&in, len2);
		dst += len2;
	}
	ffstr_set(buf, x->decrypted.ptr, dst - (byte*)x->decrypted.ptr);
	return 0;
}
