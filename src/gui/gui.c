/**
Copyright (c) 2018 Simon Zolin
*/

#include <util/gui-winapi/winapi-shell.h>
#include <gui/gui.h>
#include <util/gui-winapi/loader.h>
#include <ffbase/time.h>
#include <FFOS/file.h>
#include <FFOS/ffos-extern.h>


const struct fpm_core *core;
gui *g;

enum {
	GRP_ALL = -2,
};

enum {
	H_TITLE,
	H_USERNAME,
	H_PASSWD,
	H_URL,
	H_NOTES,
	H_MODTIME,
};

enum {
	WDB_OPLOAD = 1,
	WDB_OPSAVE,
};

#define HIDDEN_TEXT  "●●●"


static void* getctl(void *udata, const ffstr *name);
static int getcmd(void *udata, const ffstr *name);
static void pm_on_action(ffui_wnd *wnd, int id);
static void wdb_on_action(ffui_wnd *wnd, int id);

static void wmain_status(const char *s);
static int grp_current(void);
static void prep_tree(void);
static ffstr* subitem_str(fpm_dbentry *ent, int subitem);
static void grps_fill(void);

static void wdb_load(void);
static void wdb_browse(void);
static int dosave(const char *fn, const byte *key);
static void wdb_save(void);
static void wdb_close(void);

static void area_setitem(fpm_dbentry *ent, int i);
static int area_add(fpm_dbentry *ent);
static void area_filter(void);
static void area_act(void);
static void area_fill(void);
static void area_find(void);
static void area_imglist(void);
static void area_status(void);

static fpm_dbentry* ent_get(int idx);
static void ent_edit(void);
static int ent_set(fpm_dbentry *ent);
static void ent_modify(void);
static void ent_clear(void);
static void ent_del(void);
static fpm_dbentry * ent_ins(void);
static void ent_copy(uint t);


static const ffui_ldr_ctl wmain_ctls[] = {
	FFUI_LDR_CTL(wmain, wmain),
	FFUI_LDR_CTL(wmain, grps),
	FFUI_LDR_CTL(wmain, area),
	FFUI_LDR_CTL(wmain, stbar),
	FFUI_LDR_CTL(wmain, pn),
	FFUI_LDR_CTL(wmain, trayicon),
	FFUI_LDR_CTL(wmain, mm),
	FFUI_LDR_CTL_END
};

static const ffui_ldr_ctl wentry_ctls[] = {
	FFUI_LDR_CTL(wentry, wentry),
	FFUI_LDR_CTL(wentry, eEntTitle),
	FFUI_LDR_CTL(wentry, eEntUsername),
	FFUI_LDR_CTL(wentry, eEntPassword),
	FFUI_LDR_CTL(wentry, eEntUrl),
	FFUI_LDR_CTL(wentry, eEntNotes),
	FFUI_LDR_CTL(wentry, eEntTag),
	FFUI_LDR_CTL(wentry, bEntOk),
	FFUI_LDR_CTL(wentry, bEntCopyUsername),
	FFUI_LDR_CTL(wentry, bEntCopyPasswd),
	FFUI_LDR_CTL(wentry, pn_notes),
	FFUI_LDR_CTL_END
};

static const ffui_ldr_ctl wfind_ctls[] = {
	FFUI_LDR_CTL(wfind, wfind),
	FFUI_LDR_CTL(wfind, efind),
	FFUI_LDR_CTL_END
};

static const ffui_ldr_ctl wdb_ctls[] = {
	FFUI_LDR_CTL(wdb, wdb),
	FFUI_LDR_CTL(wdb, edbfn),
	FFUI_LDR_CTL(wdb, edbpass),
	FFUI_LDR_CTL(wdb, edbpass2),
	FFUI_LDR_CTL(wdb, bbrowse),
	FFUI_LDR_CTL(wdb, bdbdo),
	FFUI_LDR_CTL_END
};

static const ffui_ldr_ctl wabout_ctls[] = {
	FFUI_LDR_CTL(wabout, wabout),
	FFUI_LDR_CTL(wabout, labout),
	FFUI_LDR_CTL(wabout, lurl),
	FFUI_LDR_CTL(wabout, ico),
	FFUI_LDR_CTL_END
};

static const ffui_ldr_ctl top_ctls[] = {
	FFUI_LDR_CTL(gui, dlg),
	FFUI_LDR_CTL(gui, mfile),
	FFUI_LDR_CTL(gui, mgroup),
	FFUI_LDR_CTL(gui, mentry),
	FFUI_LDR_CTL(gui, mhelp),
	FFUI_LDR_CTL(gui, mtray),

	FFUI_LDR_CTL3(gui, wmain, wmain_ctls),
	FFUI_LDR_CTL3(gui, wentry, wentry_ctls),
	FFUI_LDR_CTL3(gui, wfind, wfind_ctls),
	FFUI_LDR_CTL3(gui, wdb, wdb_ctls),
	FFUI_LDR_CTL3(gui, wabout, wabout_ctls),
	FFUI_LDR_CTL_END
};

static const char *const scmds[] = {
	"PM_QUIT",
	"PM_ABOUT",
	"PM_SHOW",
	"PM_MINTOTRAY",
	"GRP_SEL",
	"AREA_ACTION",
	"AREA_CHSEL",
	"OPEN_HOMEPAGE",

	"DB_OPEN",
	"DB_SAVE",
	"DB_SAVEAS",
	"DB_DO",
	"DB_BROWSE",
	"DB_CLOSE",

	"GRP_DEL",

	"ENT_NEW",
	"ENT_EDIT",
	"ENT_DEL",
	"ENT_FIND",
	"ENT_FINDSHOW",
	"ENT_DONE",
	"ENT_COPYUSENAME",
	"ENT_COPYPASSWD",
	"ENT_CANCEL",
};

static void* getctl(void *udata, const ffstr *name)
{
	gui *g = udata;
	return ffui_ldr_findctl(top_ctls, g, name);
}

static int getcmd(void *udata, const ffstr *name)
{
	int i;
	for (i = 0;  i < FF_COUNT(scmds);  i++) {
		if (ffstr_eqz(name, scmds[i]))
			return i + 1;
	}
	return 0;
}

static int active_entry(fpm_dbentry **ent, int *area_idx)
{
	if (g->active_ent == NULL)
		return -1;
	*ent = g->active_ent;
	*area_idx = g->active_item;
	return 0;
}

static fpm_dbentry* ent_get(int idx)
{
	ffui_viewitem it = {};
	ffui_view_setindex(&it, idx);
	ffui_view_setparam(&it, NULL);
	if (0 != ffui_view_get(&g->wmain.area, 0, &it))
		return NULL;
	return (void*)ffui_view_param(&it);
}

static void wmain_status(const char *s)
{
	ffui_stbar_settextz(&g->wmain.stbar, 0, s);
}

/** DB entry -> GUI */
static void ent_edit(void)
{
	fpm_dbentry *ent;
	int focused = ffui_view_focused(&g->wmain.area);
	if (focused == -1)
		return;

	ent = ent_get(focused);
	if (ent == NULL)
		return;

	g->active_ent = ent;
	g->active_item = focused;

	ffui_settextstr(&g->wentry.eEntTitle, &ent->title);
	ffui_settextstr(&g->wentry.eEntUsername, &ent->username);
	ffui_settextstr(&g->wentry.eEntPassword, &ent->passwd);
	ffui_settextstr(&g->wentry.eEntUrl, &ent->url);
	ffui_settextstr(&g->wentry.eEntNotes, &ent->notes);

	if (ent->grp != -1) {
		const fpm_dbgroup *gr = dbif->grp(g->dbx, ent->grp);
		ffui_settextstr(&g->wentry.eEntTag, &gr->name);
	} else
		ffui_cleartext(&g->wentry.eEntTag);
}

/** GUI -> DB entry */
static int ent_set(fpm_dbentry *ent)
{
	ffstr tag = {};
	if (-1 == ffui_textstr(&g->wentry.eEntTitle, &ent->title))
		goto fail;
	if (-1 == ffui_textstr(&g->wentry.eEntUsername, &ent->username))
		goto fail;
	if (-1 == ffui_textstr(&g->wentry.eEntPassword, &ent->passwd))
		goto fail;
	if (-1 == ffui_textstr(&g->wentry.eEntUrl, &ent->url))
		goto fail;
	if (-1 == ffui_textstr(&g->wentry.eEntNotes, &ent->notes))
		goto fail;

	if (-1 == ffui_textstr(&g->wentry.eEntTag, &tag))
		goto fail;
	if (tag.len == 0)
		ent->grp = -1;
	else {
		fpm_dbgroup *gr;
		ent->grp = dbif->grp_find(g->dbx, tag.ptr, tag.len);

		if (ent->grp == -1) {
			fpm_dbgroup gr1;
			gr1.name = tag;
			ent->grp = dbif->grp_add(g->dbx, &gr1);
			if (ent->grp == -1)
				goto fail;
			gr = dbif->grp(g->dbx, ent->grp);
			ffstr_null(&tag);

			ffui_tvitem ti = {};
			ffui_tree_settextstr(&ti, &gr->name);
			ffui_tree_append(&g->wmain.grps, NULL, &ti);
			ffui_tree_reset(&ti);
		}
	}

	ffstr_free(&tag);
	return 0;

fail:
	ffstr_free(&tag);
	ffstr_free(&ent->title);
	ffstr_free(&ent->username);
	ffstr_free(&ent->passwd);
	ffstr_free(&ent->url);
	ffstr_free(&ent->notes);
	return 1;
}

/** GUI -> DB entry */
static void area_setitem(fpm_dbentry *ent, int i)
{
	ffui_viewitem it = {};

	ffui_view_setindex(&it, i);
	ffui_view_settextstr(&it, &ent->username);
	ffui_view_set(&g->wmain.area, H_USERNAME, &it);

	ffui_view_settextz(&it, HIDDEN_TEXT);
	ffui_view_set(&g->wmain.area, H_PASSWD, &it);

	ffui_view_settextstr(&it, &ent->url);
	ffui_view_set(&g->wmain.area, H_URL, &it);

	ffui_view_settextstr(&it, &ent->notes);
	ffui_view_set(&g->wmain.area, H_NOTES, &it);

	fftime t = {};
	t.sec = ent->mtime + FFTIME_1970_SECONDS;
	// TODO local time
	ffdatetime dt;
	char stime[32];
	fftime_split1(&dt, &t);
	size_t n = fftime_tostr1(&dt, stime, sizeof(stime), FFTIME_YMD);
	ffui_view_settext(&it, stime, n);
	ffui_view_set(&g->wmain.area, H_MODTIME, &it);
}

/** GUI -> DB */
static void ent_modify(void)
{
	ffui_viewitem it = {};
	int idx;
	fpm_dbentry *ent;
	if (0 != active_entry(&ent, &idx))
		return;

	dbif->ent(FPM_DB_MOD, g->dbx, ent);

	if (0 != ent_set(ent)) {
		wmain_status("Error");
		return;
	}

	int gcur;
	gcur = grp_current();
	if (ent->grp != gcur && gcur != GRP_ALL) {
		// this item is moved to another group.  Remove it from the list.
		ffui_view_rm(&g->wmain.area, idx);
		pm_on_action(&g->wmain.wmain, ENT_NEW);

	} else {
		ffui_view_setindex(&it, idx);
		ffui_view_settextstr(&it, &ent->title);
		ffui_view_set(&g->wmain.area, H_TITLE, &it);
		area_setitem(ent, idx);
	}

	wmain_status("Item updated");
}

static void ent_clear(void)
{
	ffui_edit *e;
	for (e = &g->wentry.eEntTitle;  e <= &g->wentry.eEntTag;  e++) {
		ffui_cleartext(e);
	}

	g->active_ent = NULL;
	g->active_item = -1;
}

static void area_status(void)
{
	char b[1024];
	size_t r = ffs_format_r0(b, FF_COUNT(b), "%u items", (int)ffui_view_nitems(&g->wmain.area));
	ffui_stbar_settext(&g->wmain.stbar, 0, b, r);
}

static void ent_del(void)
{
	fpm_dbentry *ent;
	int focused = ffui_view_focused(&g->wmain.area);
	if (focused == -1)
		return;
	ent = ent_get(focused);
	if (ent == NULL)
		return;
	ffui_view_rm(&g->wmain.area, focused);
	dbif->ent(FPM_DB_RM, g->dbx, ent);

	if (g->active_item == focused) {
		pm_on_action(&g->wmain.wmain, ENT_NEW);
	}

	area_status();
}

static int area_add(fpm_dbentry *ent)
{
	int i;
	ffui_viewitem lv = {};

	ffui_view_setparam(&lv, ent);
	ffui_view_setimg(&lv, 0);
	ffui_view_settextstr(&lv, &ent->title);
	i = ffui_view_append(&g->wmain.area, &lv);
	if (i == -1) {
		wmain_status("Error");
		return -1;
	}
	area_setitem(ent, i);
	return i;
}

static fpm_dbentry * ent_ins(void)
{
	fpm_dbentry ent, *e;
	ent.grp = -1;

	if (0 != ent_set(&ent))
		goto fail;

	e = dbif->ent(FPM_DB_INS, g->dbx, &ent);
	dbif->ent(FPM_DB_MOD, g->dbx, e);

	g->active_item = area_add(e);
	area_status();

	return e;

fail:
	wmain_status("Error");
	return NULL;
}

static void area_find(void)
{
	fpm_dbentry *ent;
	ffstr search;
	if (-1 == ffui_textstr(&g->wfind.efind, &search)) {
		wmain_status("Error");
		return;
	}

	ffui_redraw(&g->wmain.area, 0);
	ffui_view_clear(&g->wmain.area);

	for (ent = NULL;  NULL != (ent = dbif->ent(FPM_DB_NEXT, g->dbx, ent));) {
		if (0 != dbif->find(ent, search.ptr, search.len)) {
			area_add(ent);
		}
	}

	ffui_redraw(&g->wmain.area, 1);
	area_status();

	ffstr_free(&search);
}

static void area_fill(void)
{
	fpm_dbentry *ent;

	ffui_redraw(&g->wmain.area, 0);
	ffui_view_clear(&g->wmain.area);
	for (ent = NULL;  NULL != (ent = dbif->ent(FPM_DB_NEXT, g->dbx, ent));) {
		area_add(ent);
	}
	ffui_redraw(&g->wmain.area, 1);

	area_status();
}

static void prep_tree(void)
{
	ffui_tvitem ti = {};
	ffui_tree_settextz(&ti, "All");
	void *all = ffui_tree_append(&g->wmain.grps, NULL, &ti);
	ffui_tree_reset(&ti);
	ffui_tree_select(&g->wmain.grps, all);

	ffui_tree_settextz(&ti, "Untagged");
	ffui_tree_append(&g->wmain.grps, NULL, &ti);
	ffui_tree_reset(&ti);
}

static void wdb_close(void)
{
	dbif->fin(g->dbx);
	if (NULL == (g->dbx = dbif->create()))
		return;
	ffmem_free(g->saved_fn);  g->saved_fn = NULL;
	ffstr s = FFSTR_INITN(g->saved_key, sizeof(g->saved_key));
	priv_clear(s);
	g->saved_key_valid = 0;

	ent_clear();
	ffui_cleartext(&g->wfind.efind);
	ffui_view_clear(&g->wmain.area);
	ffui_tree_clear(&g->wmain.grps);
	wmain_status("");
}

static void wdb_load(void)
{
	ffstr fn = {}, qpass = {};
	int r;
	byte key[FPM_DB_KEYLEN];

	wdb_close();
	ffui_textstr(&g->wdb.edbfn, &fn);

	ffui_textstr(&g->wdb.edbpass, &qpass);
	ffui_cleartext(&g->wdb.edbpass);
	dbfif->keyinit(key, qpass.ptr, qpass.len);
	priv_clear(qpass);
	ffstr_free(&qpass);

	wmain_status("Loading database...");

	r = dbfif->open(g->dbx, fn.ptr, key);
	if (r != 0) {
		char buf[1024];
		ffs_format_r0(buf, FF_COUNT(buf), "Error: %e: %E%Z", r, fferr_last());
		wmain_status(buf);
		goto done;
	}
	ffui_show(&g->wdb.wdb, 0);

	g->saved_fn = fn.ptr;
	fn.ptr = NULL;
	memcpy(g->saved_key, key, sizeof(key));
	g->saved_key_valid = 1;

	area_fill();

	prep_tree();
	grps_fill();

done:
	ffstr_free(&fn);
}

static void grps_fill(void)
{
	fpm_dbgroup *gr;
	uint i;
	for (i = 0;  NULL != (gr = dbif->grp(g->dbx, i));  i++) {
		ffui_tvitem ti = {};
		ffui_tree_settextstr(&ti, &gr->name);
		ffui_tree_append(&g->wmain.grps, NULL, &ti);
		ffui_tree_reset(&ti);
	}
}

static int dosave(const char *fn, const byte *key)
{
	int r;

	wmain_status("Saving...");
	r = dbfif->save(g->dbx, fn, key);
	if (r != 0) {
		char buf[1024];
		ffs_format_r0(buf, FF_COUNT(buf), "Error: %e: %E%Z", r, fferr_last());
		wmain_status(buf);
		return 1;
	}
	wmain_status("Saved database");
	return 0;
}

static void wdb_browse(void)
{
	const char *fn = "";

	if (g->db_op == WDB_OPLOAD) {
		ffui_dlg_titlez(&g->dlg, "Open database");
		fn = ffui_dlg_open(&g->dlg, &g->wdb.wdb);

	} else {
		ffui_dlg_titlez(&g->dlg, "Save database");
		fn = ffui_dlg_save(&g->dlg, &g->wdb.wdb, NULL, 0);
	}

	if (fn == NULL)
		return;

	ffui_settextz(&g->wdb.edbfn, fn);
}

static void wdb_save(void)
{
	ffstr fn = {}, pass = {}, pass2 = {};
	byte key[FPM_DB_KEYLEN];

	ffui_textstr(&g->wdb.edbpass, &pass);
	ffui_textstr(&g->wdb.edbpass2, &pass2);

	if (pass.len == 0 || !ffstr_eq2(&pass, &pass2)) {
		wmain_status("Error: The passwords must match");
		goto done;
	}

	ffui_cleartext(&g->wdb.edbpass);
	ffui_cleartext(&g->wdb.edbpass2);
	dbfif->keyinit(key, pass.ptr, pass.len);
	priv_clear(pass);
	ffstr_free(&pass);
	priv_clear(pass2);
	ffstr_free(&pass2);

	ffui_textstr(&g->wdb.edbfn, &fn);
	if (0 != dosave(fn.ptr, key))
		goto done;

	ffui_show(&g->wdb.wdb, 0);

done:
	ffstr_free(&fn);
	ffstr_free(&pass);
	ffstr_free(&pass2);
}

static int grp_current(void)
{
	void *it;
	char *gname = NULL;
	int grp;

	it = ffui_tree_focused(&g->wmain.grps);
	if (it == NULL)
		return GRP_ALL;
	gname = ffui_tree_text(&g->wmain.grps, it);

	if (!ffsz_cmp(gname, "All"))
		grp = GRP_ALL;
	else if (!ffsz_cmp(gname, "Untagged"))
		grp = -1;
	else
		grp = dbif->grp_find(g->dbx, gname, ffsz_len(gname));

	ffmem_free(gname);
	return grp;
}

static void area_filter(void)
{
	uint grp = grp_current();
	fpm_dbentry *ent;

	if (grp == GRP_ALL) {
		area_fill();
		return;
	}

	ffui_redraw(&g->wmain.area, 0);
	ffui_view_clear(&g->wmain.area);

	for (ent = NULL;  NULL != (ent = dbif->ent(FPM_DB_NEXT, g->dbx, ent));) {
		if (grp == ent->grp) {
			area_add(ent);
		}
	}

	ffui_redraw(&g->wmain.area, 1);
	area_status();
}

static ffstr* subitem_str(fpm_dbentry *ent, int subitem)
{
	switch (subitem) {
	case H_TITLE:
		return &ent->title;
	case H_USERNAME:
		return &ent->username;
	case H_PASSWD:
		return &ent->passwd;
	case H_URL:
		return &ent->url;
	case H_NOTES:
		return &ent->notes;
	}
	return NULL;
}

static void area_act(void)
{
	int focused, subitem;
	ffui_point pt;
	fpm_dbentry *ent;
	ffstr *s;

	ffui_cur_pos(&pt);
	if (-1 == (focused = ffui_view_hittest(&g->wmain.area, &pt, &subitem)))
		return;

	ent = ent_get(focused);
	if (ent == NULL)
		return;

	if (NULL == (s = subitem_str(ent, subitem)))
		return;
	ffui_clipbd_set(s->ptr, s->len);
	wmain_status("Copied to clipboard");
}

static void ent_copy(uint t)
{
	ffstr s = {};
	switch (t) {
	case H_USERNAME:
		ffui_textstr(&g->wentry.eEntUsername, &s);
		break;

	case H_PASSWD:
		ffui_textstr(&g->wentry.eEntPassword, &s);
		break;
	}
	ffui_clipbd_set(s.ptr, s.len);
	ffstr_free(&s);
	wmain_status("Copied to clipboard");
}

static void wdb_on_action(ffui_wnd *wnd, int id)
{
	switch (id) {
	case DB_BROWSE:
		wdb_browse();
		break;

	case DB_DO:
		if (g->db_op == WDB_OPLOAD) {
			wdb_load();
		} else if (g->db_op == WDB_OPSAVE) {
			wdb_save();
		}
		break;
	}
}

static void pm_on_action(ffui_wnd *wnd, int id)
{
	// fflog("pm_on_action(): id: %u", id);

	switch (id) {
	case AREA_CHSEL:
	case ENT_EDIT:
	case ENT_CANCEL:
	case ENT_DONE:
		if (g->entry_creating) {
			g->entry_creating = 0;
			ffui_settextz(&g->wentry.wentry, "Edit Item");
			ffui_settext_q(g->wentry.bEntOk.h, TEXT("&Apply"));

			if (id == ENT_DONE) {
				g->active_ent = ent_ins();
				return;
			}
		}

		if (id == ENT_CANCEL) {
			ent_clear();
		}
		break;
	}

	switch (id) {

	case GRP_DEL: {
		// delete DB group.  Move all its items to Untagged group.
		int grp = grp_current();
		if (grp == -1 || grp == GRP_ALL)
			return;
		dbif->grp_del(g->dbx, grp);
		void *it = ffui_tree_focused(&g->wmain.grps);
		ffui_tree_remove(&g->wmain.grps, it);
		area_filter();
		break;
	}

	case ENT_NEW:
		g->entry_creating = 1;
		ent_clear();
		ffui_settextz(&g->wentry.wentry, "New Item");
		ffui_settext_q(g->wentry.bEntOk.h, TEXT("&Create"));
		ffui_show(&g->wentry.wentry, 1);
		ffui_wnd_setfront(&g->wentry.wentry);
		break;

	case AREA_CHSEL:
	case ENT_EDIT:
		ent_edit();
		ffui_settextz(&g->wentry.wentry, "Edit Item");
		ShowWindow(g->wentry.wentry.h, SW_SHOWNOACTIVATE);
		break;

	case ENT_DEL:
		ent_del();
		break;

	case ENT_DONE:
		ent_modify();
		break;

	case ENT_COPYUSENAME:
		ent_copy(H_USERNAME);
		break;

	case ENT_COPYPASSWD:
		ent_copy(H_PASSWD);
		break;

	case ENT_FINDSHOW:
		ffui_setfocus(&g->wfind.efind);
		ffui_show(&g->wfind.wfind, 1);
		break;

	case ENT_FIND:
		area_find();
		break;


	case DB_OPEN:
		g->db_op = WDB_OPLOAD;
		ffui_settextz(&g->wdb.wdb, "Open database");
		ffui_settextz(&g->wdb.bdbdo, "Open");
		ffui_show(&g->wdb.edbpass2, 0);
		ffui_setfocus(&g->wdb.edbfn);
		ffui_show(&g->wdb.wdb, 1);
		ffui_wnd_setfront(&g->wdb.wdb);
		break;

	case DB_SAVE:
		if (g->saved_key_valid) {
			dosave(g->saved_fn, g->saved_key);
			break;
		}
		// break

	case DB_SAVEAS:
		g->db_op = WDB_OPSAVE;
		ffui_settextz(&g->wdb.wdb, "Save database");
		ffui_settextz(&g->wdb.bdbdo, "Save");
		ffui_show(&g->wdb.edbpass2, 1);
		ffui_setfocus(&g->wdb.edbfn);
		ffui_show(&g->wdb.wdb, 1);
		ffui_wnd_setfront(&g->wdb.wdb);
		break;

	case DB_CLOSE:
		wdb_close();
		break;


	case AREA_ACTION:
		area_act();
		break;

	case GRP_SEL:
		area_filter();
		break;


	case PM_SHOW:
		ffui_show(wnd, 1);
		ffui_wnd_setfront(wnd);
		break;

	case PM_MINTOTRAY:
		ffui_show(wnd, 0);
		ffui_show(&g->wentry.wentry, 0);
		ffui_show(&g->wdb.wdb, 0);
		ffui_show(&g->wfind.wfind, 0);
		ffui_show(&g->wabout.wabout, 0);
		break;

	case PM_ABOUT:
		ffui_show(&g->wabout.wabout, 1);
		break;

	case PM_QUIT:
		ffui_wnd_close(&g->wmain.wmain);
		break;
	}
}

static void wabout_action(ffui_wnd *wnd, int id)
{
	switch (id) {
	case OPEN_HOMEPAGE:
		if (0 != ffui_shellexec(FPM_HOMEPAGE, SW_SHOWNORMAL))
			errlog("ffui_shellexec()", 0);
		break;
	}
}

static void area_imglist(void)
{
	ffui_iconlist himg;
	ffui_icon hico;

	ffui_iconlist_create(&himg, 16, 16);

	ffui_icon_load(&hico, "rundll32.exe", 0, FFUI_ICON_SMALL);
	ffui_iconlist_add(&himg, &hico);
	ffui_icon_destroy(&hico);

	ffui_view_seticonlist(&g->wmain.area, &himg);
}

static void grps_imglist(void)
{
	ffui_iconlist himg;
	ffui_icon hico;

	ffui_iconlist_create(&himg, 16, 16);

	ffui_icon_load(&hico, "shell32.dll", 3, FFUI_ICON_SMALL);
	ffui_iconlist_add(&himg, &hico);
	ffui_icon_destroy(&hico);

	ffui_tree_seticonlist(&g->wmain.grps, &himg);
}

static int gui_run(void)
{
	ffui_loader ldr;
	int r;
	char *fn;

	ffui_init();
	ffui_wnd_initstyle();
	ffui_ldr_init(&ldr);

	if (NULL == (fn = core->getpath(FFSTR("fpassman.gui")))) {
		r = 1;
		goto done;
	}
	ldr.getctl = &getctl;
	ldr.getcmd = &getcmd;
	ldr.udata = g;
	r = ffui_ldr_loadfile(&ldr, fn);
	ffmem_free(fn);

	if (r != 0) {
		ffvec msg = {};
		ffvec_addfmt(&msg, "fpassman.gui: %s\n", ffui_ldr_errstr(&ldr));
		fffile_write(ffstderr, msg.ptr, msg.len);
		ffui_msgdlg_show("fpassman", msg.ptr, msg.len, FFUI_MSGDLG_ERR);
		ffvec_free(&msg);
		ffui_ldr_fin(&ldr);
		goto done;
	}
	ffui_ldr_fin(&ldr);

	g->wmain.wmain.top = 1;
	g->wmain.wmain.on_action = g->wentry.wentry.on_action = g->wfind.wfind.on_action
		= &pm_on_action;
	g->wabout.wabout.hide_on_close = g->wentry.wentry.hide_on_close = g->wfind.wfind.hide_on_close
		= 1;

	g->wdb.wdb.on_action = &wdb_on_action;
	g->wdb.wdb.hide_on_close = 1;

	// g->wentry.eEntUsername.focus_id = g->wentry.eEntTitle.focus_id = g->wentry.eEntUrl.focus_id = entEditboxFocus;
	char buf[255];
	int n = ffs_format_r0(buf, sizeof(buf),
		"fpassman v%s\n\n"
		"Fast password manager"
		, FPM_VER);
	ffui_settext(&g->wabout.labout, buf, n);
	ffui_settextz(&g->wabout.lurl, FPM_HOMEPAGE);
	g->wabout.wabout.on_action = &wabout_action;

	area_imglist();
	grps_imglist();
	prep_tree();

	if (core->conf()->loaddb.len != 0) {
		pm_on_action(&g->wmain.wmain, DB_OPEN);
		ffui_settextstr(&g->wdb.edbfn, &core->conf()->loaddb);
		ffui_setfocus(&g->wdb.edbpass);
	}

	ffui_run();
	r = 0;

done:
	ffui_dlg_destroy(&g->dlg);
	ffui_wnd_destroy(&g->wmain.wmain);
	ffui_uninit();
	return r;
}

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	int r = 1;

	if (NULL == (g = ffmem_new(gui)))
		return -1;

	if (NULL == (core = core_init()))
		goto done;
	if (0 != core->setroot(NULL))
		goto done;

	if (0 != core->loadconf())
		goto done;

	if (NULL == (g->dbx = dbif->create()))
		goto done;

	if (0 != gui_run())
		goto done;

	r = 0;

done:
	if (g->dbx != NULL)
		dbif->fin(g->dbx);
	if (core != NULL)
		core_free();
	ffmem_free(g);  g = NULL;
	return r;
}
