/** fpassman: compress data
2018,2022 Simon Zolin
*/

#include <zlib/zlib-ff.h>

enum {
	ZBUFSIZE = 16*1024,
};

static ffvec zbufs; // ffstr[]

static void* fpm_z_alloc(void* opaque, unsigned int items, unsigned int size)
{
	ffstr *s = ffvec_pushT(&zbufs, ffstr);
	s->len = items * size;
	s->ptr = ffmem_alloc(s->len);
	return s->ptr;
}

static void fpm_z_free(void* opaque, void* address)
{
	ffstr *it;
	FFSLICE_WALK(&zbufs, it) {
		if (address == it->ptr) {
			priv_clear(*it);
			ffstr_null(it);
			break;
		}
	}
	ffmem_free(address);
}

static int compress(const ffstr *data, ffstr *compressed)
{
	z_ctx *lz = NULL;
	int r = -1, e;

	if (NULL == ffstr_alloc(compressed, data->len))
		return -1;

	z_conf conf = {};
	conf.zalloc = fpm_z_alloc;
	conf.zfree = fpm_z_free;
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
	ffvec_free(&zbufs);
	return r;
}

struct dbf_compress {
	z_ctx *lz;
	ffstr plain;
};

static int decompress_open(struct dbf_compress *x)
{
	if (NULL == ffstr_alloc(&x->plain, ZBUFSIZE))
		return -1;
	z_conf conf = {};
	conf.zalloc = fpm_z_alloc;
	conf.zfree = fpm_z_free;
	if (0 != z_inflate_init(&x->lz, &conf)) {
		errlog("z_inflate_init()", 0);
		return -1;
	}
	return 0;
}

static void decompress_close(struct dbf_compress *x)
{
	ffstr s = FFSTR_INITN(x->plain.ptr, ZBUFSIZE);
	priv_clear(s);
	ffstr_free(&x->plain);
	if (x->lz != NULL)
		z_inflate_free(x->lz);
	ffvec_free(&zbufs);
}

static int decompress_do(struct dbf_compress *x, ffstr *in, ffstr *out, int flush)
{
	size_t rd = in->len;
	int r = z_inflate(x->lz, in->ptr, &rd, x->plain.ptr, ZBUFSIZE, flush);
	ffstr_shift(in, rd);
	if (r > 0) {
		x->plain.len = r;
		ffstr_set2(out, &x->plain);
	}
	return r;
}
