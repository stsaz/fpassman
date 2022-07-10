/** Database storage format.
JSON -> deflate -> AES-CBC with SHA256 key.
Copyright (c) 2018 Simon Zolin
*/

#include <fpassman.h>
#include <ffbase/json-scheme.h>
#include <ffbase/json-writer.h>
#include <FFOS/error.h>
#include <FFOS/file.h>

#include <zlib/zlib-ff.h>
#include <aes/aes-ff.h>
#include <sha/sha256.h>
#include <sha1/sha1.h>

#define JSON_EBADVAL 100

typedef struct dbparse {
	uint ver;
	uint idx;
	fpm_db *db;
	union {
	fpm_dbentry ent;
	fpm_dbgroup grp;
	};
} dbparse;

#include <dbf-json-scheme.h>

enum {
	BUFSIZE = 4 * 1024,
};

static int compress(const ffstr *src, ffstr *compressed);
static int encrypt(fpm_db *db, ffstr *src, ffstr *dststr, const byte *key);
static int file_save(const char *fn, ffstr *dst);
static void set_iv(byte iv[16], const byte *key);

static void dbf_keyinit(byte *key, const char *passwd, size_t len)
{
	sha256_hash(passwd, len, key);
}

static int dbf_open(fpm_db *db, const char *fn, const byte *key)
{
	dbparse dbp = {};
	dbp.db = db;
	ffjson js = {};
	ffjson_scheme ps = {};
	int e = 0, flush = 0, r = JSON_EBADVAL;
	ffstr data, fbuf = {}, decrypted = {}, plain = {}, zin;
	fffd f = FFFILE_NULL;
	size_t n;
	z_ctx *lz = NULL;
	aes_ctx de = {};
	byte iv[16];
	byte *dst;
	set_iv(iv, key);

	ffjson_init(&js);
	ffjson_scheme_init(&ps, &js, 0);
	ffjson_scheme_addctx(&ps, dbf_args, &dbp);

	if (FFFILE_NULL == (f = fffile_open(fn, FFFILE_READONLY))) {
		errlog("fffile_open()", 0);
		goto done;
	}

	if (0 != aes_decrypt_init(&de, key, FPM_DB_KEYLEN, AES_CBC)) {
		errlog("aes_decrypt_init()", 0);
		goto done;
	}

	if (NULL == ffstr_alloc(&fbuf, BUFSIZE))
		goto done;
	if (NULL == ffstr_alloc(&decrypted, BUFSIZE))
		goto done;
	if (NULL == ffstr_alloc(&plain, BUFSIZE))
		goto done;

	z_conf conf = {};
	if (0 != z_inflate_init(&lz, &conf)) {
		errlog("z_inflate_init()", 0);
		goto done;
	}

	enum { R_READ, R_DECRYPT, R_DECOMPRESS, R_JSON, };
	uint state = 0;
	for (;;) {
	switch (state) {

	case R_READ:
		n = fffile_read(f, fbuf.ptr, BUFSIZE);
		if ((ssize_t)n < 0) {
			errlog("fffile_read()", 0);
			goto done;
		} else if (n == 0) {
			flush = Z_FINISH;
			state = R_DECOMPRESS;
			continue;
		}
		ffstr_set(&data, fbuf.ptr, n);
		state = R_DECRYPT;
		//fallthrough

	case R_DECRYPT:
		dst = (void*)decrypted.ptr;
		while (data.len != 0) {
			size_t len2 = ffmin(data.len, 1024);
			if (0 != aes_decrypt_chunk(&de, (byte*)data.ptr, dst, len2, iv)) {
				errlog("aes_decrypt_chunk()", 0);
				goto done;
			}
			ffstr_shift(&data, len2);
			dst += len2;
		}
		decrypted.len = n;
		zin = decrypted;
		state = R_DECOMPRESS;
		//fallthrough

	case R_DECOMPRESS: {
		if (e == Z_DONE)
			goto ok;
		size_t rd = zin.len;
		e = z_inflate(lz, zin.ptr, &rd, plain.ptr, BUFSIZE, flush);
		ffstr_shift(&zin, rd);

		if (e > 0)
		{}
		else
		switch (e) {
		case Z_DONE:
			state = R_JSON;
			continue;

		case 0:
			state = R_READ;
			continue;

		default:
			errlog("z_inflate()", 0);
			goto done;
		}

		ffstr_set(&data, plain.ptr, e);
		state = R_JSON;
		//fallthrough
	}

	case R_JSON:
		while (data.len != 0) {
			int rc = ffjson_parse(&js, &data);
			rc = ffjson_scheme_process(&ps, rc);
			if (rc < 0) {
				errlog("json parse: %s", ffjson_errstr(rc));
				goto done;
			}
		}
		state = R_DECOMPRESS;
		continue;
	}
	}

ok:
	ffmem_zero(js.buf.ptr, js.buf.len);
	r = ffjson_fin(&js);
	if (r < 0) {
		errlog("json parse: %s", ffjson_errstr(r));
		goto done;
	}

	r = 0;

done:
	ffmem_zero(plain.ptr, plain.len);
	ffstr_free(&plain);

	ffmem_zero(decrypted.ptr, decrypted.len);
	ffstr_free(&decrypted);

	ffmem_zero(js.buf.ptr, js.buf.len);
	ffjson_fin(&js);

	aes_decrypt_free(&de);
	if (lz != NULL)
		z_inflate_free(lz);
	ffstr_free(&fbuf);
	ffjson_scheme_destroy(&ps);
	fffile_close(f);
	return r;
}

static int compress(const ffstr *data, ffstr *compressed)
{
	z_ctx *lz = NULL;
	int r = -1, e;

	if (NULL == ffstr_alloc(compressed, data->len))
		return -1;

	z_conf conf = {};
	if (0 != z_deflate_init(&lz, &conf)) {
		errlog("z_deflate_init", 0);
		return -1;
	}
	size_t rd = data->len;
	e = z_deflate(lz, data->ptr, &rd, compressed->ptr, data->len, Z_FINISH);
	if (e <= 0) {
		errlog("z_deflate", 0);
		goto end;
	}
	compressed->len = e;
	r = 0;

end:
	if (lz != NULL)
		z_deflate_free(lz);
	return r;
}

static void set_iv(byte iv[16], const byte *key)
{
	byte sha1sum[SHA1_LENGTH];
	sha1_ctx sha1;
	sha1_init(&sha1);
	sha1_update(&sha1, key, FPM_DB_KEYLEN);
	sha1_fin(&sha1, sha1sum);
	memcpy(iv, sha1sum, 16);
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

	if (NULL == ffstr_alloc(&buf, BUFSIZE))
		goto done;
	dst = (void*)buf.ptr;

	if (0 != aes_encrypt_init(&en, key, FPM_DB_KEYLEN, AES_CBC))
		goto done;
	set_iv(iv, key);

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
	aes_encrypt_free(&en);
	ffstr_free(&buf);
	if (r != 0)
		ffvec_free(&d);
	return r;
}

static int file_save(const char *fn, ffstr *dst)
{
	int e = 1;
	char fntmp[4096];

	ffs_format(fntmp, sizeof(fntmp), "%s.fpassman-tmp%Z", fn);
	if (0 != fffile_writewhole(fntmp, dst->ptr, dst->len, 0)) {
		errlog("fffile_writewhole");
		goto done;
	}

	if (0 != fffile_rename(fntmp, fn))
		goto done;

	e = 0;

done:
	return e;
}

static int dbf_save(fpm_db *db, const char *fn, const byte *key)
{
	ffstr compressed = {}, encrypted = {};
	int e = 0;
	ffstr json = {}, src = {};

	json_prep(db, &json);
	src = json;

	if (0 != compress(&src, &compressed)) {
		e = JSON_EBADVAL;
		goto done;
	}

	ffstr_set(&src, compressed.ptr, compressed.len);
	if (0 != encrypt(db, &src, &encrypted, key)) {
		e = JSON_EBADVAL;
		goto done;
	}
	ffstr_free(&compressed);

	if (0 != file_save(fn, &encrypted))
		goto done;
	e = 0;

done:
	ffstr_free(&json);
	ffstr_free(&compressed);
	ffstr_free(&encrypted);
	return e;
}

static const struct fpm_dbf_iface _dbf = {
	dbf_keyinit, dbf_open, dbf_save,
};
const struct fpm_dbf_iface *dbfif = &_dbf;
