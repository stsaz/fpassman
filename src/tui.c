/** Terminal UI.
Copyright (c) 2018 Simon Zolin
*/

#include <fpassman.h>
#include <FF/data/conf.h>
#include <FF/data/psarg.h>
#include <FF/data/utf8.h>
#include <FF/path.h>
#include <FFOS/file.h>
#include <FFOS/process.h>
#include <FFOS/error.h>


struct pm {
	fpm_db *db;

	char *dbfn;
	ffstr filter;
};

static struct pm *pm;
static const struct fpm_core *core;

static int pm_cmdline(int argc, const char **argv);
static void pm_free(void);

static int pm_help(ffparser_schem *p, void *obj);


static void pm_free(void)
{
	if (pm->db != NULL)
		dbif->fin(pm->db);
	ffmem_free(pm->dbfn);
	ffmem_free(pm);
}

static const ffpars_arg pm_cmd_args[] = {
	{ "db",	FFPARS_SETVAL('d') | FFPARS_TCHARPTR | FFPARS_FCOPY | FFPARS_FSTRZ,  FFPARS_DSTOFF(struct pm, dbfn) },
	{ "filter",	FFPARS_SETVAL('f') | FFPARS_TSTR | FFPARS_FCOPY,  FFPARS_DSTOFF(struct pm, filter) },
	{ "help",	FFPARS_SETVAL('h') | FFPARS_TBOOL | FFPARS_FALONE,  FFPARS_DST(&pm_help) },
};


/** Show help info. */
static int pm_help(ffparser_schem *p, void *obj)
{
	char buf[4096];
	ssize_t n;
	char *fn;
	fffd f;

	if (NULL == (fn = core->getpath(FFSTR("help.txt"))))
		return FFPARS_ESYS;

	f = fffile_open(fn, O_RDONLY | O_NOATIME);
	ffmem_free(fn);
	if (f == FF_BADFD)
		return FFPARS_ELAST;
	n = fffile_read(f, buf, sizeof(buf));
	fffile_close(f);
	if (n > 0)
		fffile_write(ffstdout, buf, n);
	return FFPARS_ELAST;
}

/** Parse command line arguments. */
static int pm_cmdline(int argc, const char **argv)
{
	ffparser_schem ps;
	ffpsarg_parser p;
	ffpars_ctx ctx = {};
	int r = 0, ret = 1;
	ffpsarg pa;
	const char *arg;

	ffpsarg_init(&pa, argv, argc);

	ffpars_setargs(&ctx, pm, pm_cmd_args, FFCNT(pm_cmd_args));
	if (0 != ffpsarg_scheminit(&ps, &p, &ctx)) {
		errlog("cmd line parser", NULL);
		return 1;
	}

	ffpsarg_parseinit(&p);

	ffpsarg_next(&pa); //skip argv[0]

	arg = ffpsarg_next(&pa);
	if (arg == NULL) {
		pm_help(NULL, NULL);
		goto fail;
	}

	while (arg != NULL) {
		int n = 0;
		r = ffpsarg_parse(&p, arg, &n);
		if (n != 0)
			arg = ffpsarg_next(&pa);

		r = ffpsarg_schemrun(&ps);

		if (r == FFPARS_ELAST)
			goto fail;

		if (ffpars_iserr(r))
			break;
	}

	if (!ffpars_iserr(r))
		r = ffpsarg_schemfin(&ps);

	if (ffpars_iserr(r)) {
		errlog("cmd line parser: %s"
			, (r == FFPARS_ESYS) ? fferr_strp(fferr_last()) : ffpars_errstr(r));
		goto fail;
	}

	ret = 0;

fail:
	ffpsarg_destroy(&pa);
	ffpars_schemfree(&ps);
	ffpsarg_parseclose(&p);
	return ret;
}

#define COL_TITLE  "Title"
#define COL_USERNAME  "Username"
#define COL_PASSWORD  "Password"
#define COL_URL  "URL"
#define COL_NOTES  "Notes"

enum {
	COL_DEFWIDTH = 10,
};

static void ent_print(const char *filter, size_t len)
{
	fpm_dbentry *e, **it;
	fpm_dbentry emax = {0};
	ffstr3 buf = {0};
	ffarr list = {0};
	void **p;

	emax.title.len = emax.username.len = emax.passwd.len = emax.url.len = COL_DEFWIDTH;

	for (e = NULL;  NULL != (e = dbif->ent(FPM_DB_NEXT, pm->db, e)); ) {
		if (0 == dbif->find(e, filter, len))
			continue;

		ffint_setmax(emax.title.len, ffutf8_len(e->title.ptr, e->title.len));
		ffint_setmax(emax.username.len, ffutf8_len(e->username.ptr, e->username.len));
		ffint_setmax(emax.passwd.len, ffutf8_len(e->passwd.ptr, e->passwd.len));
		ffint_setmax(emax.url.len, ffutf8_len(e->url.ptr, e->url.len));

		p = ffarr_push(&list, void*);
		*p = e;
	}

	ffstr_catfmt(&buf, COL_TITLE "%*c" COL_USERNAME "%*c" COL_PASSWORD "%*c" COL_URL "%*c" COL_NOTES "\n"
		"%*c\n"
		, emax.title.len - FFSLEN(COL_TITLE) + 1, ' '
		, emax.username.len - FFSLEN(COL_USERNAME) + 1, ' '
		, emax.passwd.len - FFSLEN(COL_PASSWORD) + 1, ' '
		, emax.url.len - FFSLEN(COL_URL) + 1, ' '
		, emax.title.len + emax.username.len + emax.passwd.len + emax.url.len, '=');

	FFARR_WALKT(&list, it, fpm_dbentry*) {
		e = *it;
		ffstr_catfmt(&buf, "%S%*c%S%*c%S%*c%S%*c%S\n"
			, &e->title, emax.title.len - ffutf8_len(e->title.ptr, e->title.len) + 1, ' '
			, &e->username, emax.username.len - ffutf8_len(e->username.ptr, e->username.len) + 1, ' '
			, &e->passwd, emax.passwd.len - ffutf8_len(e->passwd.ptr, e->passwd.len) + 1, ' '
			, &e->url, emax.url.len - ffutf8_len(e->url.ptr, e->url.len) + 1, ' '
			, &e->notes);
	}

	fffile_write(ffstdout, buf.ptr, buf.len);
	ffarr_free(&buf);
	ffarr_free(&list);
}

static int tui_run()
{
	char pwd[32];
	byte key[FPM_DB_KEYLEN];
	ssize_t r;

	const char *dbfn;
	if (pm->dbfn != NULL)
		dbfn = pm->dbfn;
	else
		dbfn = core_init()->conf()->loaddb.ptr;
	if (dbfn == NULL) {
		errlog("Database file isn't specified", NULL);
		goto fail;
	}

	if (1) {
		fffile_writecz(ffstdout, "Database password:");

		ffstd_attr(ffstdin, FFSTD_ECHO, 0);
		r = fffile_read(ffstdin, pwd, sizeof(pwd));
		ffstd_attr(ffstdin, FFSTD_ECHO, FFSTD_ECHO);
		fffile_writecz(ffstdout, "\n");
		r = ffs_findeol(pwd, r);
	}

	dbfif->keyinit(key, pwd, r);
	ffmem_tzero(pwd);

	if (0 != dbfif->open(pm->db, dbfn, key)) {
		errlog("Database open failed", NULL);
		goto fail;
	}

	ent_print(pm->filter.ptr, pm->filter.len);

	dbif->fin(pm->db);
	return 0;

fail:
	return 1;
}

int main(int argc, char **argv)
{
	int r = 1;

	ffmem_init();
	if (NULL == (pm = ffmem_new(struct pm)))
		return 1;

	if (NULL == (core = core_init()))
		goto done;
	if (0 != core->setroot(argv[0]))
		goto done;

	if (0 != pm_cmdline(argc, (const char**)argv))
		goto done;

	if (0 != core->loadconf())
		goto done;

	if (NULL == (pm->db = dbif->create()))
		goto done;

	if (0 != tui_run())
		goto done;

	r = 0;

done:
	pm_free();
	if (core != NULL)
		core_free();
	return r;
}
