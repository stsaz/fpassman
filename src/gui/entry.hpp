/** fpassman
2018, Simon Zolin */

typedef struct wentry {
	ffui_windowxx	wnd;
	ffui_labelxx	ltitle, luser, lpw, lurl, lgrp, lnotes;
	ffui_editxx		title, username, password, url, tag;
	ffui_textxx		notes;
	ffui_buttonxx	b_ok, copy_username, copy_passwd;
#ifdef FF_WIN
	ffui_paned		pn_notes;
#endif
} wentry;

#define _(m)  FFUI_LDR_CTL(wentry, m)
const ffui_ldr_ctl wentry_ctls[] = {
	_(wnd),
	_(ltitle), _(luser), _(lpw), _(lurl), _(lgrp), _(lnotes),
	_(title),
	_(username),
	_(password),
	_(url),
	_(notes),
	_(tag),
	_(b_ok),
	_(copy_username),
	_(copy_passwd),
#ifdef FF_WIN
	_(pn_notes),
#endif
	FFUI_LDR_CTL_END
};
#undef _

/** DB entry -> GUI */
void wentry_fill(const fpm_dbentry *ent)
{
	g->wentry->title.text(ent->title);
	g->wentry->username.text(ent->username);
	g->wentry->password.text(HIDDEN_TEXT);
	g->wentry->url.text(ent->url);
	g->wentry->notes.clear();
	g->wentry->notes.add(ent->notes);

	if (ent->grp != -1) {
		const fpm_dbgroup *gr = dbif->grp(g->dbx, ent->grp);
		g->wentry->tag.text(gr->name);
	} else {
		g->wentry->tag.text("");
	}

	g->wentry->wnd.title("Edit Item");
#ifdef FF_WIN
	ShowWindow(g->wentry->wnd.h, SW_SHOWNOACTIVATE);
#else
	g->wentry->wnd.show(1);
#endif
}

static void ent_copy(uint id)
{
	xxvec v;
	xxstr s;
	switch (id) {
	case ENT_COPY_USERNAME:
		s = v.acquire(g->wentry->username.text()).str();  break;

	case ENT_COPY_PASSWORD:
		if (!g->active_ent)
			return;
		s = g->active_ent->passwd;
		break;
	}
	ffui_clipboard_settextstr(&s);
	wmain_status("Copied to clipboard");
}

void wentry_clear()
{
	g->wentry->title.text("");
	g->wentry->username.text("");
	g->wentry->password.text("");
	g->wentry->url.text("");
	g->wentry->notes.clear();
	g->wentry->tag.text("");
	g->active_ent = NULL;
	g->active_item = -1;
}

/** GUI -> DB entry */
void wentry_set(fpm_dbentry *ent)
{
	ent->title = g->wentry->title.text();
	ent->username = g->wentry->username.text();
	ent->passwd = g->wentry->password.text();
	ent->url = g->wentry->url.text();
	ent->notes = g->wentry->notes.text();
	xxvec tag = g->wentry->tag.text();
	if (tag.len == 0)
		ent->grp = -1;
	else {
		fpm_dbgroup *gr;
		ent->grp = dbif->grp_find(g->dbx, (char*)tag.ptr, tag.len);

		if (ent->grp == -1) {
			fpm_dbgroup gr1;
			gr1.name = tag.str();
			ent->grp = dbif->grp_add(g->dbx, &gr1);
			gr = dbif->grp(g->dbx, ent->grp);
			tag.reset();

			wmain_grp_add(gr->name);
		}
	}
}

void wentry_update()
{
	if (g->entry_creating) {
		g->entry_creating = 0;
		g->wentry->wnd.title("Edit Item");
		g->wentry->b_ok.text("&Apply");
	}
}

void wentry_new()
{
	g->entry_creating = 1;
	wentry_clear();
	g->wentry->wnd.title("New Item");
	g->wentry->b_ok.text("&Create");
	ffui_show(&g->wentry->wnd, 1);
	g->wentry->wnd.present();
}

static void wentry_action(ffui_window *wnd, int id)
{
	switch (id) {
	case ENT_CANCEL:
		wentry_update();
		wentry_clear();
		break;

	case ENT_DONE:
		if (g->entry_creating) {
			g->entry_creating = 0;
			g->wentry->wnd.title("Edit Item");
			g->wentry->b_ok.text("&Apply");

			fpm_dbentry ent;
			ent.grp = -1;
			wentry_set(&ent);
			ent_ins(&ent);
			break;
		}
		ent_modify();
		break;

	case ENT_COPY_USERNAME:
	case ENT_COPY_PASSWORD:
		ent_copy(id);  break;
	}
}

void wentry_show(uint show)
{
	ffui_show(&g->wentry->wnd, show);
}

void wentry_init()
{
	g->wentry = ffmem_new(struct wentry);
	g->wentry->wnd.on_action = wentry_action;
	g->wentry->wnd.hide_on_close = 1;
	// g->wentry->username.focus_id = g->wentry->title.focus_id = g->wentry->url.focus_id = entEditboxFocus;
}
