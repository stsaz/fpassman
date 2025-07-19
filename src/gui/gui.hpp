/**
2018 Simon Zolin
*/

#include <fpassman.h>
#ifdef FF_WIN
	#include <ffgui/winapi/loader.h>
#else
	#include <ffgui/gtk/loader.h>
#endif
#include <ffgui/loader.h>
#include <ffgui/gui.hpp>
#include <util/util.hpp>

struct wmain;
void wmain_show();
void wmain_init();
void wmain_filter(int grp_id, ffstr search);
void wmain_status(const char *s);
void wmain_grp_add(const char *name);

struct wdb;
void wdb_init();
void wdb_open(uint flags);
void wdb_saveas();
void wdb_show(uint show);

struct wentry;
void wentry_init();
void wentry_fill(const fpm_dbentry *ent);
void wentry_new();
void wentry_clear();
void wentry_update();
void wentry_set(fpm_dbentry *ent);

struct wfind;
void wfind_init();
void wfind_clear();
void wfind_show(uint show);

struct wabout;
void wabout_init();
void wabout_show(int show);

extern const ffui_ldr_ctl
	wmain_ctls[],
	wdb_ctls[],
	wentry_ctls[],
	wfind_ctls[],
	wabout_ctls[];

typedef struct gui {
	ffui_dialogxx dlg;

	ffui_menu mfile, mgroup, mentry, mhelp;

	struct wmain *wmain;
	fpm_dbentry *active_ent;
	int active_item;

	struct wentry *wentry;
	uint entry_creating :1;

	struct wfind *wfind;
	xxvec cur_filter;

	struct wdb *wdb;
	uint db_op;
	char *saved_fn;
	byte saved_key[32];
	uint saved_key_valid :1;
	xxvec items; // fpm_dbentry*[]

	struct wabout *wabout;

	fpm_db *dbx;
} gui;

extern gui *g;

#define A(id)  id
enum CMDS {
	A_NONE,
	#include "actions.h"
};
#undef A
