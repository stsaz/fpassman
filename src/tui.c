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
	ffstr add_data[5];
	uint add_data_n;
	ffstr filter;
	ffbyte add_entry_mode;
};

static struct pm *pm;
static const struct fpm_core *core;

static int pm_cmdline(int argc, const char **argv);
static void pm_free(void);

static int pm_help(ffcmdarg_scheme *p, void *obj);


#define FFARRAY_FOREACH(static_array, it) \
	for (it = static_array;  it != static_array + FF_COUNT(static_array);  it++)

static void pm_free(void)
{
	if (pm->db != NULL)
		dbif->fin(pm->db);
	ffstr *it;
	FFARRAY_FOREACH(pm->add_data, it) {
		ffstr_free(it);
	}
	ffmem_free(pm->dbfn);
	ffmem_free(pm);
}

static int pm_arg(ffcmdarg_scheme *p, void *obj, ffstr *s)
{
	if (!pm->add_entry_mode)
		return -FFCMDARG_ERROR;
	if (pm->add_data_n == FF_COUNT(pm->add_data))
		return -FFCMDARG_ERROR;
	ffstr_dupstr(&pm->add_data[pm->add_data_n], s);
	pm->add_data_n++;
	return 0;
}

static const ffcmdarg_arg pm_cmd_args[] = {
	{ 0, "",	FFCMDARG_TSTR,  (ffsize)pm_arg },
	{ 'd', "db",    FFCMDARG_TSTRZ,  FF_OFF(struct pm, dbfn) },
	{ 'f', "filter",	FFCMDARG_TSTR,  FF_OFF(struct pm, filter) },
	{ 'a', "add",    FFCMDARG_TSWITCH,  FF_OFF(struct pm, add_entry_mode) },
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

void ent_log(const fpm_dbentry *ent)
{
	fflog("%S %S %S %S %S"
		, &ent->title, &ent->username, &ent->passwd, &ent->url, &ent->notes);
}

void ent_add(ffstr *ss, uint n)
{
	if (n == 0)
		return;

	fpm_dbentry ent = {};
	ent.grp = -1;

	fftime t;
	fftime_now(&t);
	ent.mtime = t.sec;

	if (n > 0)
		ffstr_dupstr(&ent.title, &ss[0]);
	if (n > 1)
		ffstr_dupstr(&ent.username, &ss[1]);
	if (n > 2)
		ffstr_dupstr(&ent.passwd, &ss[2]);
	if (n > 3)
		ffstr_dupstr(&ent.url, &ss[3]);
	if (n > 4)
		ffstr_dupstr(&ent.notes, &ss[4]);
	fpm_dbentry *it = dbif->ent(FPM_DB_INS, pm->db, &ent);
	ent_log(it);
}

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

int pwd_get(char *pwd, ffsize n)
{
	ffstdout_fmt("Database password:");

	ffstd_attr(ffstdin, FFSTD_ECHO, 0);
	int r = fffile_read(ffstdin, pwd, n);
	ffstd_attr(ffstdin, FFSTD_ECHO, FFSTD_ECHO);
	ffstdout_fmt("\n");

	ffstr t = FFSTR_INITN(pwd, r);
	ffstr_splitby(&t, '\n', &t, NULL);
	ffstr_rskipchar1(&t, '\r');
	return t.len;
}

static int tui_run()
{
	int rc = 0;
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

	char pwd[128];
	r = pwd_get(pwd, sizeof(pwd));
	dbfif->keyinit(key, pwd, r);
	ffmem_zero_obj(pwd);

	if (0 != dbfif->open(pm->db, dbfn, key)) {
		errlog("Database open failed", NULL);
		goto fail;
	}

	if (pm->add_entry_mode) {
		ent_add(pm->add_data, pm->add_data_n);
		if (0 != dbfif->save(pm->db, dbfn, key)) {
			errlog("Database save failed", NULL);
			goto fail;
		}
	} else {
		ent_print(pm->filter.ptr, pm->filter.len);
	}

	rc = 0;

fail:
	dbif->fin(pm->db);
	return rc;
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
