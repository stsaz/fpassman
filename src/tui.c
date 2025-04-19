/** Terminal UI.
2018 Simon Zolin
*/

#include <fpassman.h>
#include <ffsys/file.h>
#include <ffsys/process.h>
#include <ffsys/error.h>
#include <ffsys/globals.h>
#include <ffbase/args.h>

#define CMD_ELAST 100

struct pm {
	fpm_db *db;

	char *dbfn;
	ffstr add_data[6]; // [ID] NAME USER PASS URL NOTES
	uint add_data_n;
	ffstr filter;
	byte add_entry_mode;
	byte edit_entry_mode;
	byte del_entry_mode;
};

static struct pm *pm;
static const struct fpm_core *core;

#define FFARRAY_FOREACH(static_array, it) \
	for (it = static_array;  it != static_array + FF_COUNT(static_array);  it++)

static void pm_free(void)
{
	if (pm->db != NULL)
		dbif->fin(pm->db);
	ffstr *it;
	FFARRAY_FOREACH(pm->add_data, it) {
		priv_clear(*it);
		ffstr_free(it);
	}
	ffmem_free(pm->dbfn);
	ffmem_free(pm);
}

static int pm_arg(void *obj, ffstr s)
{
	if (!(pm->add_entry_mode || pm->edit_entry_mode || pm->del_entry_mode))
		return -FFARGS_E_ARG;
	if (pm->add_data_n == FF_COUNT(pm->add_data))
		return -FFARGS_E_ARG;
	ffstr_dupstr(&pm->add_data[pm->add_data_n], &s);
	pm->add_data_n++;
	return 0;
}

/** Show help info. */
static int pm_help()
{
	static const char help[] = "\
Usage:\n\
\n\
    fpassman [OPTIONS] [-a NAME USER PASS URL NOTES] [-e ID NAME USER PASS URL NOTES] [-r ID]\n\
\n\
Options:\n\
\n\
 -db DBFILE         Set database file\n\
 -filter STR        Set filter\n\
\n\
 -add NAME USER PASS URL NOTES\n\
                    Add new entry\n\
 -edit ID NAME USER PASS URL NOTES\n\
                    Edit existing entry\n\
 -remove ID         Remove existing row by its ID\n\
\n\
 -help              Print help info and exit\n\
";

	ffstdout_write(help, FFS_LEN(help));
	return CMD_ELAST;
}

#define O(m)  (void*)FF_OFF(struct pm, m)
static const struct ffarg pm_cmd_args[] = {
	{ "-add",		'1',	O(add_entry_mode) },
	{ "-db",		'=s',	O(dbfn) },
	{ "-edit",		'1',	O(edit_entry_mode) },
	{ "-filter",	'S',	O(filter) },
	{ "-help",		0,		pm_help },
	{ "-remove",	'1',	O(del_entry_mode) },
	{ "\0\1",		'S',	pm_arg },
	{}
};
#undef O

/** Parse command line arguments. */
static int pm_cmdline(int argc, char **argv)
{
	char *cmd_line = NULL;
	struct ffargs a = {};
	int r = 0, ret = 1;

	if (argc == 1) {
		pm_help(NULL, NULL);
		goto end;
	}

	uint flags = FFARGS_O_PARTIAL | FFARGS_O_DUPLICATES;

#ifdef FF_WIN

	cmd_line = ffsz_alloc_wtou(GetCommandLineW());
	ffstr line = FFSTR_INITZ(cmd_line), arg;
	_ffargs_next(&line, &arg); // skip exe name
	char *cmd_line1 = line.ptr;
	r = ffargs_process_line(&a, pm_cmd_args, pm, flags, cmd_line1);

#else

	r = ffargs_process_argv(&a, pm_cmd_args, pm, flags, argv + 1, argc - 1);

#endif

	if (r == CMD_ELAST)
		goto end;
	else if (r < 0) {
		errlog("%s", a.error);
		goto end;
	}

	ret = 0;

end:
	ffmem_free(cmd_line);
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
	fflog("%u %S %S %S %S %S"
		, ent->id
		, &ent->title, &ent->username, &ent->passwd, &ent->url, &ent->notes);
}

int ent_add_edit(ffstr *ss, uint n, uint to_add)
{
	if ((to_add && n < 1)
		|| (!to_add && n < 2)) {
		errlog("not enough arguments");
		return -1;
	}

	fpm_dbentry ent = {};
	ent.grp = -1;

	fftime t;
	fftime_now(&t);
	ent.mtime = t.sec;

	if (!to_add) {
		uint u;
		if (!ffstr_to_uint32(&ss[0], &u)) {
			errlog("bad ID");
			return -1;
		}
		ent.id = u;
		ss++;
	}

	if (n > 0)
		ent.title = ss[0];
	if (n > 1)
		ent.username = ss[1];
	if (n > 2)
		ent.passwd = ss[2];
	if (n > 3)
		ent.url = ss[3];
	if (n > 4)
		ent.notes = ss[4];

	uint m = FPM_DB_ADD;
	if (!to_add) {
		m = FPM_DB_SETBYID;
	}

	fpm_dbentry *it = dbif->ent(m, pm->db, &ent);
	ent_log(it);
	return 0;
}

static int ent_del(ffstr *ss, uint n)
{
	if (n != 1) {
		errlog("expecting ID of the row to delete");
		return -1;
	}

	fpm_dbentry ent = {};

	uint u;
	if (!ffstr_to_uint32(&ss[0], &u)) {
		errlog("bad ID");
		return -1;
	}
	ent.id = u;

	dbif->ent(FPM_DB_RMBYID, pm->db, &ent);
	return 0;
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

	ffvec_addfmt(&buf, "ID      " COL_TITLE "%*c" COL_USERNAME "%*c" COL_PASSWORD "%*c" COL_URL "%*c" COL_NOTES "\n"
		"%*c\n"
		, emax.title.len - FFS_LEN(COL_TITLE) + 1, ' '
		, emax.username.len - FFS_LEN(COL_USERNAME) + 1, ' '
		, emax.passwd.len - FFS_LEN(COL_PASSWORD) + 1, ' '
		, emax.url.len - FFS_LEN(COL_URL) + 1, ' '
		, emax.title.len + emax.username.len + emax.passwd.len + emax.url.len, '=');

	FFSLICE_WALK(&list, it) {
		e = *it;
		ffvec_addfmt(&buf, "%7u %S%*c%S%*c%S%*c%S%*c%S\n"
			, e->id
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
	ffstr s = FFSTR_INITN(pwd, sizeof(pwd));
	priv_clear(s);

	uint flags = 0;
	if (0 != dbfif->open2(pm->db, dbfn, key, flags)) {
		errlog("Database open failed", NULL);
		goto fail;
	}

	if (pm->add_entry_mode || pm->edit_entry_mode || pm->del_entry_mode) {

		if (pm->add_entry_mode || pm->edit_entry_mode) {
			if (0 != ent_add_edit(pm->add_data, pm->add_data_n, pm->add_entry_mode)) {
				goto fail;
			}
		} else if (pm->del_entry_mode) {
			if (0 != ent_del(pm->add_data, pm->add_data_n))
				goto fail;
		}

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

	fflog("fpassman v" FPM_VER);

	if (!(pm = ffmem_new(struct pm)))
		return 1;

	if (!(core = core_init()))
		goto done;
	if (core->setroot(argv[0]))
		goto done;

	if (pm_cmdline(argc, argv))
		goto done;

	if (core->loadconf())
		goto done;

	if (!(pm->db = dbif->create()))
		goto done;

	if (tui_run())
		goto done;

	r = 0;

done:
	pm_free();
	if (core)
		core_free();
	return r;
}
