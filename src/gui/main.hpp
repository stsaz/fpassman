/** fpassman
2018, Simon Zolin */

typedef struct wmain {
	ffui_windowxx		wnd;
	ffui_menu			mm;
	ffui_labelxx		lgrp;
	ffui_comboboxxx		cbgroups;
	ffui_viewxx			area;
	ffui_statusbarxx	stbar;

	int group_id_current;
} wmain;

static void groups_fill();

#define _(m)  FFUI_LDR_CTL(wmain, m)
const ffui_ldr_ctl wmain_ctls[] = {
	_(wnd),
	_(lgrp), _(cbgroups),
	_(area),
	_(stbar),
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

static int active_entry(fpm_dbentry **ent, int *area_idx)
{
	if (g->active_ent == NULL)
		return -1;
	*ent = g->active_ent;
	*area_idx = g->active_item;
	return 0;
}

static fpm_dbentry* entry_at(uint i)
{
	return *g->items.at<fpm_dbentry*>(i);
}

void wmain_status(const char *s)
{
	g->wmain->stbar.text(s);
}

static void ent_modify()
{
	int idx;
	fpm_dbentry *ent;
	if (0 != active_entry(&ent, &idx))
		return;

	dbif->ent(FPM_DB_MOD, g->dbx, ent);

	wentry_set(ent);
	g->wmain->area.update(idx, 0);
	wmain_status("Item updated");
}

static void area_status()
{
	g->wmain->stbar.text(xxstr_buf<1000>().zfmt("%u items", (int)g->items.len));
}

static void ent_del()
{
	int focused = g->wmain->area.selected_first();
	if (focused == -1)
		return;

	fpm_dbentry *ent = entry_at(focused);
	dbif->ent(FPM_DB_RM, g->dbx, ent);

	g->items.remove<fpm_dbentry*>(focused, 1);
	g->wmain->area.length(g->items.len, 1);

	if (g->active_item == focused) {
		wentry_new();
	}

	area_status();
}

static void ent_edit()
{
	fpm_dbentry *ent;
	int focused = g->wmain->area.selected_first();
	if (focused == -1)
		return;

	if (!(ent = entry_at(focused)))
		return;

	g->active_ent = ent;
	g->active_item = focused;

	wentry_fill(ent);
}

static void ent_ins(fpm_dbentry *ent)
{
	fpm_dbentry *e = dbif->ent(FPM_DB_INS, g->dbx, ent);
	dbif->ent(FPM_DB_MOD, g->dbx, e);

	g->active_item = g->items.len;
	*g->items.push<fpm_dbentry*>() = e;
	g->active_ent = e;
	g->wmain->area.length(g->items.len, 1);
	area_status();
}

void wmain_filter(int grp_id, ffstr search)
{
	if (grp_id == -1)
		grp_id = g->wmain->group_id_current;

	g->items.len = 0;

	fpm_dbentry *ent = NULL;
	while ((ent = dbif->ent(FPM_DB_NEXT, g->dbx, ent))) {
		if ((grp_id == GRP_ALL || grp_id == ent->grp)
			&& (search.len == 0
				|| dbif->find(ent, search.ptr, search.len))) {
			*g->items.push<fpm_dbentry*>() = ent;
		}
	}

	g->wmain->area.length(g->items.len, 1);
	area_status();
}

static void wdb_close()
{
	dbif->fin(g->dbx);
	g->dbx = dbif->create();
	ffmem_free(g->saved_fn);  g->saved_fn = NULL;
	ffstr s = FFSTR_INITN(g->saved_key, sizeof(g->saved_key));
	priv_clear(s);
	g->saved_key_valid = 0;

	g->wmain->cbgroups.clear();
	g->items.free();
	g->wmain->area.clear();
	wentry_clear();
	wfind_clear();
	wmain_status("");
}

static int dosave(const char *fn, const byte *key)
{
	int r;

	wmain_status("Saving...");
	r = dbfif->save(g->dbx, fn, key);
	if (r != 0) {
		char buf[1024];
		ffs_format_r0(buf, FF_COUNT(buf), "Error: %d: %E%Z", r, fferr_last());
		wmain_status(buf);
		return 1;
	}
	wmain_status("Saved database");
	return 0;
}

static void group_select(int i)
{
	if (i == 0)
		i = GRP_ALL;
	else
		i--;

	g->wmain->group_id_current = i;
	wmain_filter(i, g->cur_filter.str());
}

static void group_delete(int id)
{
	if (id == GRP_ALL)
		return;
	dbif->grp_del(g->dbx, id);
	groups_fill();
	wmain_filter(GRP_ALL, g->cur_filter.str());
}

static void area_display(ffui_viewxx_disp *d)
{
	const fpm_dbentry *ent = entry_at(d->index());
	char buf[32];
	xxstr s;

	switch (d->subindex()) {
	case H_TITLE:
		s = ent->title;  break;

	case H_USERNAME:
		s = ent->username;  break;

	case H_PASSWD:
		s = FFSTR_Z(HIDDEN_TEXT);  break;

	case H_URL:
		s = ent->url;  break;

	case H_NOTES:
		s = ent->notes;  break;

	case H_MODTIME: {
		fftime t = {};
		t.sec = ent->mtime + FFTIME_1970_SECONDS;
		// TODO local time
		ffdatetime dt;
		fftime_split1(&dt, &t);
		size_t n = fftime_tostr1(&dt, buf, sizeof(buf), FFTIME_YMD);
		s.set(buf, n);
		break;
	}
	}

	d->text(s);
}

static void wmain_action(ffui_window *wnd, int id)
{
	switch (id) {

	case AREA_DISPLAY:
		area_display((ffui_viewxx_disp*)g->wmain->area.dispinfo_item);  break;

	case ENT_NEW:
		wentry_new();  break;

	case AREA_CHSEL:
	case ENT_EDIT:
		wentry_update();
		ent_edit();
		break;

	case ENT_DEL:
		ent_del();  break;

	case ENT_FIND_SHOW:
		wfind_show(1);  break;


	case DB_OPEN:
		wdb_open(0);  break;

	case DB_SAVE:
		if (g->saved_key_valid) {
			dosave(g->saved_fn, g->saved_key);
			break;
		}
		// fallthrough

	case DB_SAVEAS:
		wdb_saveas();  break;

	case DB_CLOSE:
		wdb_close();  break;


	case AREA_ACTION:
		break;

	case GRP_SEL:
		group_select(g->wmain->cbgroups.get());  break;

	case GRP_DEL:
		group_delete(g->wmain->group_id_current);  break;


	case PM_ABOUT_SHOW:
		wabout_show(1);  break;

	case PM_QUIT:
		g->wmain->wnd.close();  break;

	case PM_EXIT:
		ffui_post_quitloop();  break;
	}
}

static void groups_fill()
{
	g->wmain->group_id_current = GRP_ALL;
	g->wmain->cbgroups.clear();
	g->wmain->cbgroups.add("All");

	fpm_dbgroup *gr;
	for (uint i = 0;  (gr = dbif->grp(g->dbx, i));  i++) {
		g->wmain->cbgroups.add(gr->name.ptr);
	}

	g->wmain->cbgroups.set(0);
}

void wmain_grp_add(const char *name)
{
	g->wmain->cbgroups.add(name);
}

void wmain_db_loaded()
{
	groups_fill();
	wmain_filter(-1, FFSTR_Z(""));
}

void wmain_show()
{
	g->wmain->wnd.show(1);
}

void wmain_init()
{
	g->wmain = ffmem_new(struct wmain);
#ifdef FF_WIN
	g->wmain->wnd.top = 1;
#endif
	g->wmain->wnd.on_action = wmain_action;
	g->wmain->wnd.onclose_id = PM_EXIT;
	g->wmain->area.dispinfo_id = AREA_DISPLAY;
}
