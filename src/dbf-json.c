/** Database storage format.
JSON -> deflate -> AES256-CBC with SHA256 key.
Copyright (c) 2018 Simon Zolin
*/

#include <fpassman.h>
#include <ffbase/json-scheme.h>
#include <ffbase/json-writer.h>
#include <FFOS/error.h>
#include <FFOS/file.h>

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
#include <dbf-crypt.h>
#include <dbf-compress.h>

enum {
	BUFSIZE = 4 * 1024,
};

static void dbf_keyinit(byte *key, const char *passwd, size_t len)
{
	sha256_hash(passwd, len, key);
}

static int dbf_open2(fpm_db *db, const char *fn, const byte *key, uint flags)
{
	dbparse dbp = {};
	dbp.db = db;
	ffjson js = {};
	ffjson_scheme ps = {};
	int e = 0, flush = 0, r = JSON_EBADVAL;
	ffstr data = {}, fbuf = {}, zin = {};
	fffd f = FFFILE_NULL;
	size_t n;
	struct dbf_crypt cr = {};
	struct dbf_compress z = {};

	ffjson_init(&js);
	ffjson_scheme_init(&ps, &js, 0);
	ffjson_scheme_addctx(&ps, dbf_args, &dbp);

	if (FFFILE_NULL == (f = fffile_open(fn, FFFILE_READONLY))) {
		errlog("fffile_open()", 0);
		goto done;
	}

	if (0 != decrypt_init(flags, &cr, key))
		goto done;
	if (0 != decompress_open(&z))
		goto done;

	if (NULL == ffstr_alloc(&fbuf, BUFSIZE))
		goto done;

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
		if (0 != decrypt(&cr, data, &zin))
			goto done;
		state = R_DECOMPRESS;
		//fallthrough

	case R_DECOMPRESS: {
		if (e == Z_DONE)
			goto ok;
		e = decompress_do(&z, &zin, &data, flush);
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
	priv_clear(*(ffstr*)&js.buf);
	r = ffjson_fin(&js);
	if (r < 0) {
		errlog("json parse: %s", ffjson_errstr(r));
		goto done;
	}

	r = 0;

done:
	priv_clear(*(ffstr*)&js.buf);
	ffjson_fin(&js);

	decrypt_close(&cr);
	decompress_close(&z);
	ffstr_free(&fbuf);
	ffjson_scheme_destroy(&ps);
	fffile_close(f);
	return r;
}

static int dbf_open(fpm_db *db, const char *fn, const byte *key)
{
	return dbf_open2(db, fn, key, 0);
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

	if (0 != file_save(fn, &encrypted))
		goto done;
	e = 0;

done:
	priv_clear(json);
	ffstr_free(&json);
	priv_clear(compressed);
	ffstr_free(&compressed);
	ffstr_free(&encrypted);
	return e;
}

static const struct fpm_dbf_iface _dbf = {
	dbf_keyinit, dbf_open, dbf_open2, dbf_save,
};
const struct fpm_dbf_iface *dbfif = &_dbf;
