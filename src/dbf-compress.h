/** fpassman: compress data
2018,2022 Simon Zolin
*/

#include <zlib/zlib-ff.h>

enum {
	ZBUFSIZE = 16*1024,
};

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

struct dbf_compress {
	z_ctx *lz;
	ffstr plain;
};

static int compress_open(struct dbf_compress *x)
{
	if (NULL == ffstr_alloc(&x->plain, ZBUFSIZE))
		return -1;
	z_conf conf = {};
	if (0 != z_inflate_init(&x->lz, &conf)) {
		errlog("z_inflate_init()", 0);
		return -1;
	}
	return 0;
}

static void compress_close(struct dbf_compress *x)
{
	ffmem_zero(x->plain.ptr, x->plain.len);
	ffstr_free(&x->plain);
	if (x->lz != NULL)
		z_inflate_free(x->lz);
}

static int compress_do(struct dbf_compress *x, ffstr *in, ffstr *out, int flush)
{
	size_t rd = in->len;
	int r = z_inflate(x->lz, in->ptr, &rd, x->plain.ptr, ZBUFSIZE, flush);
	ffstr_shift(in, rd);
	if (r > 0)
		ffstr_set(out, x->plain.ptr, r);
	return r;
}
