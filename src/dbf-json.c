/** Database storage format.
JSON -> deflate -> AES-CBC with SHA256 key.
Copyright (c) 2018 Simon Zolin
*/

/* File format:
{
ver:1,

groups:[
	[name]
],

entries:[
	[title, username, password, url, notes, groupID]
]
}
*/

#include <fpassman.h>
#include <FF/data/json.h>
#include <FFOS/error.h>
#include <FFOS/file.h>

#include <zlib/zlib-ff.h>
#include <aes/aes.h>
#include <sha/sha256.h>
#include <sha1/sha1.h>


typedef struct dbparse {
	uint ver;
	uint idx;
	fpm_db *db;
	union {
	fpm_dbentry ent;
	fpm_dbgroup grp;
	};
} dbparse;

enum {
	BUFSIZE = 4 * 1024,

	//0..4
	I_MTIME = 5,
	I_GROUP,
};

// DBF interface
static void dbf_keyinit(byte *key, const char *passwd, size_t len);
static int dbf_open(fpm_db *db, const char *fn, const byte *key);
static int dbf_save(fpm_db *db, const char *fn, const byte *key);
static const struct fpm_dbf_iface _dbf = {
	&dbf_keyinit, &dbf_open, &dbf_save,
};
const struct fpm_dbf_iface *dbfif = &_dbf;

static int compress(const ffstr *src, ffstr *compressed);
static int encrypt(fpm_db *db, ffstr *src, ffstr *dststr, const byte *key);
static int file_save(const char *fn, ffstr *dst);
static void set_iv(byte iv[16], const byte *key);

// JSON scheme
static int grp_item(ffparser_schem *ps, void *obj, const ffstr *val);
static int grp_done(ffparser_schem *ps, void *obj);
static const ffpars_arg grp_args[] = {
	{ NULL, FFPARS_TSTR | FFPARS_FCOPY, FFPARS_DST(&grp_item) },
	{ NULL, FFPARS_TCLOSE, FFPARS_DST(&grp_done) },
};
static int new_grp(ffparser_schem *ps, void *obj, ffpars_ctx *ctx);
static const ffpars_arg grps_args[] = {
	{ NULL, FFPARS_TARR, FFPARS_DST(&new_grp) }
};

static int ent_item(ffparser_schem *ps, void *obj, const ffstr *val);
static int ent_item_int(ffparser_schem *ps, void *obj, const int64 *val);
static int ent_done(ffparser_schem *ps, void *obj);
static const ffpars_arg ent_args[] = {
	{ NULL, FFPARS_TSTR | FFPARS_FCOPY, FFPARS_DST(&ent_item) },
	{ NULL, FFPARS_TINT | FFPARS_FSIGN, FFPARS_DST(&ent_item_int) },
	{ NULL, FFPARS_TCLOSE, FFPARS_DST(&ent_done) },
};
static int new_ent(ffparser_schem *ps, void *obj, ffpars_ctx *ctx);
static const ffpars_arg ents_args[] = {
	{ NULL, FFPARS_TARR, FFPARS_DST(&new_ent) }
};

static int dbf_ver(ffparser_schem *ps, void *obj, const int64 *val);
static int dbf_groups(ffparser_schem *ps, void *obj, ffpars_ctx *ctx);
static int dbf_ents(ffparser_schem *ps, void *obj, ffpars_ctx *ctx);
static const ffpars_arg dbf_args[] = {
	{ "ver", FFPARS_TINT, FFPARS_DST(&dbf_ver) },
	{ "groups", FFPARS_TARR, FFPARS_DST(&dbf_groups) },
	{ "entries", FFPARS_TARR, FFPARS_DST(&dbf_ents) },
};


static int grp_done(ffparser_schem *ps, void *obj)
{
	dbparse *dbp = obj;
	dbif->grp_add(dbp->db, &dbp->grp);
	return 0;
}

static int grp_item(ffparser_schem *ps, void *obj, const ffstr *val)
{
	dbparse *dbp = obj;
	if (dbp->idx != 0)
		return FFPARS_EBADVAL;
	dbp->idx++;
	ffstr_set(&dbp->grp.name, val->ptr, val->len);
	return 0;
}

static int new_grp(ffparser_schem *ps, void *obj, ffpars_ctx *ctx)
{
	dbparse *dbp = obj;
	ffmem_tzero(&dbp->grp);
	ffpars_setargs(ctx, obj, grp_args, FFCNT(grp_args));
	dbp->idx = 0;
	return 0;
}


static int ent_item(ffparser_schem *ps, void *obj, const ffstr *val)
{
	dbparse *dbp = obj;
	fpm_dbentry *ent = &dbp->ent;
	switch (dbp->idx) {
	case 0:
		ffstr_set2(&ent->title, val);
		break;
	case 1:
		ffstr_set2(&ent->username, val);
		break;
	case 2:
		ffstr_set2(&ent->passwd, val);
		break;
	case 3:
		ffstr_set2(&ent->url, val);
		break;
	case 4:
		ffstr_set2(&ent->notes, val);
		break;

	default:
		return FFPARS_EBADVAL;
	}

	dbp->idx++;
	return 0;
}

static int ent_item_int(ffparser_schem *ps, void *obj, const int64 *val)
{
	dbparse *dbp = obj;
	fpm_dbentry *ent = &dbp->ent;
	switch (dbp->idx) {
	case I_MTIME:
		if ((uint)*val != *val)
			return FFPARS_EBIGVAL;
		ent->mtime = (uint)*val;
		break;

	case I_GROUP:
		if (*val == -1) {
			dbp->idx++;
			return 0;
		}
		ent->grp = (int)*val;
		break;

	default:
		return FFPARS_EBADVAL;
	}

	dbp->idx++;
	return 0;
}

static int ent_done(ffparser_schem *ps, void *obj)
{
	dbparse *dbp = obj;
	dbif->ent(FPM_DB_INS, dbp->db, &dbp->ent);
	return 0;
}

static int new_ent(ffparser_schem *ps, void *obj, ffpars_ctx *ctx)
{
	dbparse *dbp = obj;
	ffmem_tzero(&dbp->ent);
	ffpars_setargs(ctx, obj, ent_args, FFCNT(ent_args));
	dbp->idx = 0;
	dbp->ent.grp = -1;
	return 0;
}

static int dbf_ver(ffparser_schem *ps, void *obj, const int64 *val)
{
	dbparse *dbp = obj;
	if (*val != 1)
		return FFPARS_EBADVAL;
	dbp->ver = 1;
	return 0;
}

static int dbf_groups(ffparser_schem *ps, void *obj, ffpars_ctx *ctx)
{
	// dbparse *dbp = obj;
	ffpars_setargs(ctx, obj, grps_args, FFCNT(grps_args));
	return 0;
}

static int dbf_ents(ffparser_schem *ps, void *obj, ffpars_ctx *ctx)
{
	// dbparse *dbp = obj;
	ffpars_setargs(ctx, obj, ents_args, FFCNT(ents_args));
	return 0;
}

static void dbf_keyinit(byte *key, const char *passwd, size_t len)
{
	sha256_buffer(passwd, len, key);
}

static int dbf_open(fpm_db *db, const char *fn, const byte *key)
{
	enum { R_READ, R_DECRYPT, R_DECOMPRESS, R_JSON, };
	dbparse dbp = {};
	ffjson js;
	uint state = 0;
	int rc = FFPARS_ENOVAL, e = 0, flush = 0, r = FFERR_INTERNAL;
	ffparser_schem ps;
	const ffpars_ctx ctx = { &dbp, dbf_args, FFCNT(dbf_args), NULL };
	const ffpars_arg top = { NULL, FFPARS_TOBJ | FFPARS_FPTR, FFPARS_DST(&ctx) };
	ffstr data, fbuf = {}, decrypted = {}, plain = {}, zin;
	fffd f = FF_BADFD;
	size_t n;
	z_ctx *lz = NULL;
	aes_decrypt_ctx de;
	byte iv[16];
	byte *dst;
	set_iv(iv, key);

	dbp.db = db;
	ffjson_scheminit(&ps, &js, &top);

	if (FF_BADFD == (f = fffile_open(fn, O_RDONLY)))
		return FFERR_FOPEN;

	ffmem_zero(&de, sizeof(aes_decrypt_ctx));
	if (0 != aes_decrypt_key(key, FPM_DB_KEYLEN, &de)) {
		errlog("aes_decrypt_key()", 0);
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

	for (;;) {
	switch (state) {

	case R_READ:
		n = fffile_read(f, fbuf.ptr, BUFSIZE);
		if ((ssize_t)n < 0) {
			r = FFERR_READ;
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
			int len2 = (int)ffmin(data.len, 1024);
			if (0 != aes_cbc_decrypt((byte*)data.ptr, dst, len2, iv, &de)) {
				errlog("aes_cbc_decrypt()", 0);
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
			size_t len = data.len;
			rc = ffjson_parse(&js, data.ptr, &len);
			ffstr_shift(&data, len);
			rc = ffjson_schemrun(&ps);
			if (ffpars_iserr(rc)) {
				errlog("json parse: %s", ffpars_errstr(rc));
				goto done;
			}
		}
		state = R_DECOMPRESS;
		continue;
	}
	}

ok:
	r = ffjson_schemfin(&ps);
	if (ffpars_iserr(r)) {
		errlog("json parse: %s", ffpars_errstr(r));
		goto done;
	}

	r = 0;

done:
	FF_SAFECLOSE(lz, NULL, z_inflate_free);
	ffarr_zero(&plain);
	ffstr_free(&plain);
	ffarr_zero(&decrypted);
	ffstr_free(&decrypted);
	ffstr_free(&fbuf);
	ffarr_zero(&js.buf);
	ffjson_parseclose(&js);
	ffpars_schemfree(&ps);
	fffile_safeclose(f);
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
	FF_SAFECLOSE(lz, NULL, z_deflate_free);
	return r;
}

static void set_iv(byte iv[16], const byte *key)
{
	byte sha1sum[SHA1_LENGTH];
	sha1_ctx sha1;
	sha1_init(&sha1);
	sha1_update(&sha1, key, FPM_DB_KEYLEN);
	sha1_fin(&sha1, sha1sum);
	ffmemcpy(iv, sha1sum, 16);
}

static int encrypt(fpm_db *db, ffstr *src, ffstr *dststr, const byte *key)
{
	aes_encrypt_ctx en;
	byte iv[16];
	byte *dst;
	ffstr buf = {};
	ffarr d = {};
	char tmp[1024];

	if (NULL == ffstr_alloc(&buf, BUFSIZE))
		goto done;
	dst = (void*)buf.ptr;

	ffmem_zero(&en, sizeof(aes_encrypt_ctx));
	if (0 != aes_encrypt_key(key, FPM_DB_KEYLEN, &en))
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

		if (0 != aes_cbc_encrypt((byte*)src->ptr, dst, (int)len2, iv, &en))
			goto done;
		ffstr_shift(src, len2);
		if (NULL == ffarr_grow(&d, len2, 512))
			goto done;
		ffarr_append(&d, buf.ptr, len2);
	}

	ffstr_set(dststr, d.ptr, d.len);
	return 0;

done:
	ffstr_free(&buf);
	ffarr_free(&d);
	return 1;
}

static int file_save(const char *fn, ffstr *dst)
{
	int e = 1;
	char fntmp[FF_MAXPATH];
	fffd fd = FF_BADFD;

	ffs_fmt(fntmp, fntmp + sizeof(fntmp), "%s.fpassman-tmp%Z", fn);

	fd = fffile_open(fntmp, O_CREAT | O_WRONLY);
	if (fd == FF_BADFD) {
		e = FFERR_FOPEN;
		goto done;
	}
	if (dst->len != fffile_write(fd, dst->ptr, dst->len)) {
		e = FFERR_WRITE;
		goto done;
	}
	if (0 != fffile_close(fd)) {
		e = FFERR_WRITE;
		goto done;
	}
	fd = FF_BADFD;

	if (0 != fffile_rename(fntmp, fn))
		goto done;

	e = 0;

done:
	fffile_safeclose(fd);
	return e;
}

static const int jstypes[] = {
	FFJSON_TARR,
	FFJSON_TSTR,
	FFJSON_TSTR,
	FFJSON_TSTR,
	FFJSON_TSTR,
	FFJSON_TSTR,
	FFJSON_TINT | FFJSON_F32BIT,
	FFJSON_TINT | FFJSON_F32BIT,
	FFJSON_TARR,
};

static int dbf_save(fpm_db *db, const char *fn, const byte *key)
{
	fpm_dbgroup *grp;
	fpm_dbentry *ent;
	ffjson_cook js;
	ffstr fbuf = {}, compressed = {}, encrypted = {};
	int e = 0;

	if (NULL == ffstr_alloc(&fbuf, BUFSIZE))
		return FFERR_BUFALOC;
	ffjson_cookinit(&js, fbuf.ptr, BUFSIZE);


	ffjson_bufadd(&js, FFJSON_TOBJ, FFJSON_CTXOPEN);

	ffjson_bufadd(&js, FFJSON_FKEYNAME, "ver");
	{
		int i = 1;
		ffjson_bufadd(&js, FFJSON_TINT | FFJSON_F32BIT, &i);
	}

	ffjson_bufadd(&js, FFJSON_FKEYNAME, "groups");
	ffjson_bufadd(&js, FFJSON_TARR, FFJSON_CTXOPEN);

	for (size_t i = 0;  NULL != (grp = dbif->grp(db, i));  i++) {
		ffjson_bufadd(&js, FFJSON_TARR, FFJSON_CTXOPEN);
		ffjson_bufadd(&js, FFJSON_TSTR, &grp->name);
		ffjson_bufadd(&js, FFJSON_TARR, FFJSON_CTXCLOSE);
	}


	ffjson_bufadd(&js, FFJSON_TARR, FFJSON_CTXCLOSE);

	ffjson_bufadd(&js, FFJSON_FKEYNAME, "entries");
	ffjson_bufadd(&js, FFJSON_TARR, FFJSON_CTXOPEN);

	for (ent = NULL;  NULL != (ent = dbif->ent(FPM_DB_NEXT, db, ent));) {
		ffjson_bufaddv(&js, jstypes, FFCNT(jstypes)
			, FFJSON_CTXOPEN
			, &ent->title
			, &ent->username
			, &ent->passwd
			, &ent->url
			, &ent->notes
			, &ent->mtime
			, &ent->grp
			, FFJSON_CTXCLOSE
			);
	}

	ffjson_bufadd(&js, FFJSON_TARR, FFJSON_CTXCLOSE);

	ffjson_bufadd(&js, FFJSON_TOBJ, FFJSON_CTXCLOSE);

	ffstr src;
	ffstr_set(&src, js.buf.ptr, js.buf.len);
	if (0 != compress(&src, &compressed)) {
		e = FFERR_INTERNAL;
		goto done;
	}
	ffjson_cookfinbuf(&js);

	ffstr_set(&src, compressed.ptr, compressed.len);
	if (0 != encrypt(db, &src, &encrypted, key)) {
		e = FFERR_INTERNAL;
		goto done;
	}
	ffstr_free(&compressed);

	if (0 != file_save(fn, &encrypted))
		goto done;
	e = 0;

done:
	ffjson_cookfinbuf(&js);
	ffstr_free(&compressed);
	ffstr_free(&encrypted);

	return e;
}
