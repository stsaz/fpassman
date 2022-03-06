/** Core module.
Copyright (c) 2018 Simon Zolin
*/

#include <fpassman.h>
#include <util/conf2-scheme.h>
#include <FFOS/process.h>
#include <FFOS/path.h>


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


static const ffconf_arg conf_args[] = {
	{ "load_db",	FFCONF_TSTRZ,  FF_OFF(struct fpm_conf, loaddb_z) },
	{}
};


static int core_loadconf(void)
{
	int r = -1;
	char *fn = NULL;
	ffstr errmsg = {};

	if (NULL == (fn = core_getpath(FFSTR("fpassman.conf"))))
		goto done;
	r = ffconf_parse_file(conf_args, &pm->conf, fn, 0, &errmsg);
	if (r < 0) {
		ffstderr_fmt("fpassman.conf: %S\n", &errmsg);
		goto done;
	}

	if (pm->conf.loaddb_z != NULL)
		ffstr_setz(&pm->conf.loaddb, pm->conf.loaddb_z);

	r = 0;

done:
	ffmem_free(fn);
	ffstr_free(&errmsg);
	return r;
}

static char* core_getpath(const char *name, size_t len)
{
	return ffsz_allocfmt("%S%*s", &pm->root, len, name);
}

static int core_setroot(const char *argv0)
{
	char fn[4096];
	ffstr path;
	const char *p = ffps_filename(fn, sizeof(fn), argv0);
	if (p == NULL)
		return 1;
	if (ffpath_splitpath(p, ffsz_len(p), &path, NULL) >= 0) {
		path.len += FFS_LEN("/");
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
	ffmem_free(pm->conf.loaddb_z);
	ffstr_free(&pm->root);
	ffmem_free(pm);
}
