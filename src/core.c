/** Core module.
Copyright (c) 2018 Simon Zolin
*/

#include <fpassman.h>
#include <FF/data/conf.h>
#include <FF/path.h>
#include <FFOS/process.h>


const struct fpm_core* core_init();
void core_free();

// CORE
static int core_setroot(const char *argv0);
static char* core_getpath(const char *name, size_t len);
static struct fpm_conf* core_conf();
static int core_loadconf(void);
static const struct fpm_core core_iface = {
	&core_setroot, &core_getpath, &core_conf, &core_loadconf
};


struct corectx {
	struct fpm_conf conf;
	ffstr root;
};
static struct corectx *pm;


static const ffpars_arg conf_args[] = {
	{ "load_db",	FFPARS_TSTR | FFPARS_FCOPY | FFPARS_FSTRZ,  FFPARS_DSTOFF(struct fpm_conf, loaddb) },
};


static int core_loadconf(void)
{
	ffconf cnf;
	ffparser_schem ps;
	ffstr s;
	ffpars_ctx ctx = {};
	int r = -1, rc = FFPARS_ENOVAL;
	char *fn, *buf = NULL;
	fffd f = FF_BADFD;

	ffpars_setargs(&ctx, &pm->conf, conf_args, FFCNT(conf_args));
	if (0 != ffconf_scheminit(&ps, &cnf, &ctx))
		goto done;

	if (NULL == (buf = ffmem_alloc(4096)))
		goto done;

	if (NULL == (fn = core_getpath(FFSTR("fpassman.conf"))))
		goto done;
	if (FF_BADFD == (f = fffile_open(fn, O_RDONLY)))
		goto done;
	ffmem_free(fn);

	for (;;) {
		ssize_t n = fffile_read(f, buf, 4096);
		if (n == -1)
			goto done;
		else if (n == 0)
			break;
		ffstr_set(&s, buf, n);

		while (s.len != 0) {
			size_t len = s.len;
			rc = ffconf_parse(&cnf, s.ptr, &len);
			ffstr_shift(&s, len);
			rc = ffconf_schemrun(&ps);
			if (ffpars_iserr(rc)) {
				fffile_fmt(ffstderr, NULL, "fpassman.conf: %s\n", ffpars_errstr(rc));
				goto done;
			}
		}
	}

	if (0 != ffconf_schemfin(&ps))
		goto done;

	r = 0;

done:
	fffile_safeclose(f);
	ffmem_safefree(buf);
	ffconf_parseclose(&cnf);
	ffpars_schemfree(&ps);
	return r;
}

static char* core_getpath(const char *name, size_t len)
{
	ffstr3 s = {0};
	if (0 == ffstr_catfmt(&s, "%S%*s%Z", &pm->root, len, name)) {
		ffarr_free(&s);
		return NULL;
	}
	return s.ptr;
}

static int core_setroot(const char *argv0)
{
	char fn[FF_MAXPATH];
	ffstr path;
	const char *p = ffps_filename(fn, sizeof(fn), argv0);
	if (p == NULL)
		return 1;
	if (NULL != ffpath_split2(p, ffsz_len(p), &path, NULL)) {
		path.len += FFSLEN("/");
		if (NULL == ffstr_dup(&pm->root, path.ptr, path.len))
			return 1;
	}
	return 0;
}

static struct fpm_conf* core_conf()
{
	return &pm->conf;
}

const struct fpm_core* core_init()
{
	if (pm == NULL) {
		pm = ffmem_new(struct corectx);
	}
	return &core_iface;
}

void core_free(void)
{
	ffstr_free(&pm->conf.loaddb);
	ffstr_free(&pm->root);
	ffmem_free(pm);
}
