/** Terminal UI.
Copyright (c) 2018 Simon Zolin
*/

#include <fpassman.h>
#include <util/cmdarg-scheme.h>
#include <FFOS/file.h>
#include <FFOS/process.h>
#include <FFOS/error.h>
#include <FFOS/ffos-extern.h>

#define CMD_ELAST 100

struct pm {
	fpm_db *db;

	char *dbfn;
	ffstr filter;
};

static struct pm *pm;
static const struct fpm_core *core;

static int pm_cmdline(int argc, const char **argv);
static void pm_free(void);

static int pm_help(ffcmdarg_scheme *p, void *obj);


static void pm_free(void)
{
	if (pm->db != NULL)
		dbif->fin(pm->db);
	ffmem_free(pm->dbfn);
	ffmem_free(pm);
}

static const ffcmdarg_arg pm_cmd_args[] = {
	{ 'd', "db",	FFCMDARG_TSTRZ,  FF_OFF(struct pm, dbfn) },
	{ 'f', "filter",	FFCMDARG_TSTR,  FF_OFF(struct pm, filter) },
	{ 'h', "help",	FFCMDARG_TSWITCH,  (ffsize)pm_help },
	{}
};


/** Show help info. */
static int pm_help(ffcmdarg_scheme *p, void *obj)
{
	char buf[4096];
	ssize_t n;
	char *fn = NULL;
	fffd f = FFFILE_NULL;

	if (NULL == (fn = core->getpath(FFSTR("help.txt"))))
		goto done;

	f = fffile_open(fn, FFFILE_READONLY);
	if (f == FFFILE_NULL)
		goto done;

	n = fffile_read(f, buf, sizeof(buf));
	if (n > 0)
		ffstdout_write(buf, n);

done:
	fffile_close(f);
	ffmem_free(fn);
	return CMD_ELAST;
}

/** Parse command line arguments. */
static int pm_cmdline(int argc, const char **argv)
{
	int r = 0, ret = 1;
	ffstr errmsg = {};

	if (argc == 1) {
		pm_help(NULL, NULL);
		goto fail;
	}

	r = ffcmdarg_parse_object(pm_cmd_args, pm, argv, argc, 0, &errmsg);
	if (r == -CMD_ELAST)
		goto fail;
	if (r < 0) {
		errlog("cmd line parser: %S", &errmsg);
		goto fail;
	}

	ret = 0;

fail:
	ffstr_free(&errmsg);
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

/** Get characters N */
int ffutf8_len(const char *d, ffsize n)
{
	int r = ffutf8_to_utf16(NULL, 0, d, n, FFUNICODE_UTF16LE);
	if (r < 0)
		return 0;
	return r / 2;
}

/** Set the maximum value.
The same as: dst = max(dst, src) */
#define ffint_setmax(dst, src) \
do { \
	if ((dst) < (src)) \
		(dst) = (src); \
} while (0)

static void ent_print(const char *filter, size_t len)
{
	fpm_dbentry *e, **it;
	fpm_dbentry emax = {};
	ffvec buf = {};
	ffvec list = {};
	void **p;

	emax.title.len = emax.username.len = emax.passwd.len = emax.url.len = COL_DEFWIDTH;

	for (e = NULL;  NULL != (e = dbif->ent(FPM_DB_NEXT, pm->db, e)); ) {
		if (0 == dbif->find(e, filter, len))
			continue;

		ffint_setmax(emax.title.len, ffutf8_len(e->title.ptr, e->title.len));
		ffint_setmax(emax.username.len, ffutf8_len(e->username.ptr, e->username.len));
		ffint_setmax(emax.passwd.len, ffutf8_len(e->passwd.ptr, e->passwd.len));
		ffint_setmax(emax.url.len, ffutf8_len(e->url.ptr, e->url.len));

		p = ffvec_pushT(&list, void*);
		*p = e;
	}

	ffvec_addfmt(&buf, COL_TITLE "%*c" COL_USERNAME "%*c" COL_PASSWORD "%*c" COL_URL "%*c" COL_NOTES "\n"
		"%*c\n"
		, emax.title.len - FFS_LEN(COL_TITLE) + 1, ' '
		, emax.username.len - FFS_LEN(COL_USERNAME) + 1, ' '
		, emax.passwd.len - FFS_LEN(COL_PASSWORD) + 1, ' '
		, emax.url.len - FFS_LEN(COL_URL) + 1, ' '
		, emax.title.len + emax.username.len + emax.passwd.len + emax.url.len, '=');

	FFSLICE_WALK(&list, it) {
		e = *it;
		ffvec_addfmt(&buf, "%S%*c%S%*c%S%*c%S%*c%S\n"
			, &e->title, emax.title.len - ffutf8_len(e->title.ptr, e->title.len) + 1, ' '
			, &e->username, emax.username.len - ffutf8_len(e->username.ptr, e->username.len) + 1, ' '
			, &e->passwd, emax.passwd.len - ffutf8_len(e->passwd.ptr, e->passwd.len) + 1, ' '
			, &e->url, emax.url.len - ffutf8_len(e->url.ptr, e->url.len) + 1, ' '
			, &e->notes);
	}

	ffstdout_write(buf.ptr, buf.len);
	ffvec_free(&buf);
	ffvec_free(&list);
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
		ffstdout_fmt("Database password:");

		ffstd_attr(ffstdin, FFSTD_ECHO, 0);
		r = fffile_read(ffstdin, pwd, sizeof(pwd));
		ffstd_attr(ffstdin, FFSTD_ECHO, FFSTD_ECHO);
		ffstdout_fmt("\n");

		ffstr t = FFSTR_INITN(pwd, r);
		ffstr_splitby(&t, '\n', &t, NULL);
		ffstr_rskipchar1(&t, '\r');
		r = t.len;
	}

	dbfif->keyinit(key, pwd, r);
	ffmem_zero_obj(pwd);

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
