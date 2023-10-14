/** fpassman
2018, Simon Zolin */

typedef struct wmain {
	ffui_windowxx		wnd;
	ffui_menu			mm;
	ffui_viewxx			grps;
	ffui_viewxx			area;
	ffui_statusbarxx	stbar;
	ffui_paned			pn;
	ffui_trayicon		trayicon;
} wmain;

#define _(m)  FFUI_LDR_CTL(wmain, m)
const ffui_ldr_ctl wmain_ctls[] = {
	_(wnd),
	_(grps),
	_(area),
	_(stbar),
	_(pn),
	_(trayicon),
	_(mm),
	FFUI_LDR_CTL_END
};
#undef _

#define HIDDEN_TEXT  "●●●"

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

static int grp_current()
{
	void *it;
	char *gname = NULL;
	int grp;

	it = ffui_tree_focused(&g->wmain->grps);
	if (it == NULL)
		return GRP_ALL;
	gname = ffui_tree_text(&g->wmain->grps, it);

	if (!ffsz_cmp(gname, "All"))
		grp = GRP_ALL;
	else if (!ffsz_cmp(gname, "Untagged"))
		grp = -1;
	else
		grp = dbif->grp_find(g->dbx, gname, ffsz_len(gname));

	ffmem_free(gname);
	return grp;
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
	if (0 != ffui_view_get(&g->wmain->area, 0, &it))
		return NULL;
	return (fpm_dbentry*)ffui_view_param(&it);
}

void wmain_status(const char *s)
{
	g->wmain->stbar.text(s);
}

/** GUI -> DB entry */
static void area_setitem(fpm_dbentry *ent, int i)
{
	ffui_viewitem it = {};

	ffui_view_setindex(&it, i);
	ffui_view_settextstr(&it, &ent->username);
	ffui_view_set(&g->wmain->area, H_USERNAME, &it);

	ffui_view_settextz(&it, HIDDEN_TEXT);
	ffui_view_set(&g->wmain->area, H_PASSWD, &it);

	ffui_view_settextstr(&it, &ent->url);
	ffui_view_set(&g->wmain->area, H_URL, &it);

	ffui_view_settextstr(&it, &ent->notes);
	ffui_view_set(&g->wmain->area, H_NOTES, &it);

	fftime t = {};
	t.sec = ent->mtime + FFTIME_1970_SECONDS;
	// TODO local time
	ffdatetime dt;
	char stime[32];
	fftime_split1(&dt, &t);
	size_t n = fftime_tostr1(&dt, stime, sizeof(stime), FFTIME_YMD);
	ffui_view_settext(&it, stime, n);
	ffui_view_set(&g->wmain->area, H_MODTIME, &it);
}

void wmain_grp_add(ffstr name)
{
	ffui_tvitem ti = {};
	ffui_tree_settextstr(&ti, &name);
	ffui_tree_append(&g->wmain->grps, NULL, &ti);
	ffui_tree_reset(&ti);
}

/** GUI -> DB */
static void ent_modify()
{
	ffui_viewitem it = {};
	int idx;
	fpm_dbentry *ent;
	if (0 != active_entry(&ent, &idx))
		return;

	dbif->ent(FPM_DB_MOD, g->dbx, ent);

	if (0 != wentry_set(ent)) {
		wmain_status("Error");
		return;
	}

	int gcur;
	gcur = grp_current();
	if (ent->grp != gcur && gcur != GRP_ALL) {
		// this item is moved to another group.  Remove it from the list.
		ffui_view_rm(&g->wmain->area, idx);
		wentry_new();

	} else {
		ffui_view_setindex(&it, idx);
		ffui_view_settextstr(&it, &ent->title);
		ffui_view_set(&g->wmain->area, H_TITLE, &it);
		area_setitem(ent, idx);
	}

	wmain_status("Item updated");
}

static void area_status()
{
	ffstrxx_buf<1000> s;
	g->wmain->stbar.text(s.zfmt("%u items", (int)ffui_view_nitems(&g->wmain->area)));
}

static void ent_del()
{
	fpm_dbentry *ent;
	int focused = ffui_view_focused(&g->wmain->area);
	if (focused == -1)
		return;
	ent = ent_get(focused);
	if (ent == NULL)
		return;
	ffui_view_rm(&g->wmain->area, focused);
	dbif->ent(FPM_DB_RM, g->dbx, ent);

	if (g->active_item == focused) {
		wentry_new();
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
	i = ffui_view_append(&g->wmain->area, &lv);
	if (i == -1) {
		wmain_status("Error");
		return -1;
	}
	area_setitem(ent, i);
	return i;
}

void wmain_filter(ffstr search)
{
	ffui_redraw(&g->wmain->area, 0);
	ffui_view_clear(&g->wmain->area);

	for (fpm_dbentry *ent = NULL;  NULL != (ent = dbif->ent(FPM_DB_NEXT, g->dbx, ent));) {
		if (0 != dbif->find(ent, search.ptr, search.len)) {
			area_add(ent);
		}
	}

	ffui_redraw(&g->wmain->area, 1);
	area_status();
}

static void ent_edit()
{
	fpm_dbentry *ent;
	int focused = ffui_view_focused(&g->wmain->area);
	if (focused == -1)
		return;

	if (!(ent = ent_get(focused)))
		return;

	g->active_ent = ent;
	g->active_item = focused;

	wentry_fill(ent);
}

static fpm_dbentry * ent_ins()
{
	fpm_dbentry ent, *e;
	ent.grp = -1;

	if (0 != wentry_set(&ent))
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

static void area_fill()
{
	fpm_dbentry *ent;

	ffui_redraw(&g->wmain->area, 0);
	ffui_view_clear(&g->wmain->area);
	for (ent = NULL;  NULL != (ent = dbif->ent(FPM_DB_NEXT, g->dbx, ent));) {
		area_add(ent);
	}
	ffui_redraw(&g->wmain->area, 1);

	area_status();
}

static void prep_tree()
{
	ffui_tvitem ti = {};
	ffui_tree_settextz(&ti, "All");
	void *all = ffui_tree_append(&g->wmain->grps, NULL, &ti);
	ffui_tree_reset(&ti);
	ffui_tree_select(&g->wmain->grps, all);

	ffui_tree_settextz(&ti, "Untagged");
	ffui_tree_append(&g->wmain->grps, NULL, &ti);
	ffui_tree_reset(&ti);
}

static void wdb_close()
{
	dbif->fin(g->dbx);
	if (NULL == (g->dbx = dbif->create()))
		return;
	ffmem_free(g->saved_fn);  g->saved_fn = NULL;
	ffstr s = FFSTR_INITN(g->saved_key, sizeof(g->saved_key));
	priv_clear(s);
	g->saved_key_valid = 0;

	wentry_clear();
	wfind_clear();
	ffui_view_clear(&g->wmain->area);
	ffui_tree_clear(&g->wmain->grps);
	wmain_status("");
}

static void grps_fill()
{
	fpm_dbgroup *gr;
	uint i;
	for (i = 0;  NULL != (gr = dbif->grp(g->dbx, i));  i++) {
		ffui_tvitem ti = {};
		ffui_tree_settextstr(&ti, &gr->name);
		ffui_tree_append(&g->wmain->grps, NULL, &ti);
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

static void area_filter()
{
	int grp = grp_current();
	fpm_dbentry *ent;

	if (grp == GRP_ALL) {
		area_fill();
		return;
	}

	ffui_redraw(&g->wmain->area, 0);
	ffui_view_clear(&g->wmain->area);

	for (ent = NULL;  NULL != (ent = dbif->ent(FPM_DB_NEXT, g->dbx, ent));) {
		if (grp == ent->grp) {
			area_add(ent);
		}
	}

	ffui_redraw(&g->wmain->area, 1);
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

static void area_act()
{
	int focused, subitem;
	ffui_point pt;
	fpm_dbentry *ent;
	ffstr *s;

	ffui_cur_pos(&pt);
	if (-1 == (focused = ffui_view_hittest(&g->wmain->area, &pt, &subitem)))
		return;

	ent = ent_get(focused);
	if (ent == NULL)
		return;

	if (NULL == (s = subitem_str(ent, subitem)))
		return;
	ffui_clipbd_set(s->ptr, s->len);
	wmain_status("Copied to clipboard");
}

static void wmain_action(ffui_wnd *wnd, int id)
{
	switch (id) {

	case GRP_DEL: {
		// delete DB group.  Move all its items to Untagged group.
		int grp = grp_current();
		if (grp == -1 || grp == GRP_ALL)
			return;
		dbif->grp_del(g->dbx, grp);
		void *it = ffui_tree_focused(&g->wmain->grps);
		ffui_tree_remove(&g->wmain->grps, it);
		area_filter();
		break;
	}

	case ENT_NEW:
		wentry_new();  break;

	case AREA_CHSEL:
	case ENT_EDIT:
		wentry_update();
		ent_edit();
		break;

	case ENT_DEL:
		ent_del();  break;

	case ENT_FINDSHOW:
		wfind_show(1);  break;


	case DB_OPEN:
		wdb_open(0);  break;

	case DB_SAVE:
		if (g->saved_key_valid) {
			dosave(g->saved_fn, g->saved_key);
			break;
		}
		// break

	case DB_SAVEAS:
		wdb_saveas();  break;

	case DB_CLOSE:
		wdb_close();  break;


	case AREA_ACTION:
		area_act();  break;

	case GRP_SEL:
		area_filter();  break;


	case PM_SHOW:
		g->wmain->wnd.show(1);
		ffui_wnd_setfront(wnd);
		break;

	case PM_MINTOTRAY:
		g->wmain->wnd.show(0);
		wdb_show(0);
		wentry_show(0);
		wfind_show(0);
		wabout_show(0);
		break;

	case PM_ABOUT:
		wabout_show(1);  break;

	case PM_QUIT:
		ffui_wnd_close(&g->wmain->wnd);  break;
	}
}

static void area_imglist()
{
	ffui_iconlist himg;
	ffui_icon hico;

	ffui_iconlist_create(&himg, 16, 16);

	ffui_icon_load(&hico, "rundll32.exe", 0, FFUI_ICON_SMALL);
	ffui_iconlist_add(&himg, &hico);
	ffui_icon_destroy(&hico);

	ffui_view_seticonlist(&g->wmain->area, &himg);
}

static void grps_imglist()
{
	ffui_iconlist himg;
	ffui_icon hico;

	ffui_iconlist_create(&himg, 16, 16);

	ffui_icon_load(&hico, "shell32.dll", 3, FFUI_ICON_SMALL);
	ffui_iconlist_add(&himg, &hico);
	ffui_icon_destroy(&hico);

	ffui_tree_seticonlist(&g->wmain->grps, &himg);
}

void wmain_show()
{
	ffui_show(&g->wmain->wnd, 1);
	ffui_tray_show(&g->wmain->trayicon, 1);

	area_imglist();
	grps_imglist();
	prep_tree();
}

void wmain_init()
{
	g->wmain = ffmem_new(struct wmain);
	g->wmain->wnd.top = 1;
	g->wmain->wnd.on_action = wmain_action;
}
