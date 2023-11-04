/** fpassman
2018, Simon Zolin */

typedef struct wentry {
	ffui_windowxx	wnd;
	ffui_editxx		title;
	ffui_editxx		username;
	ffui_editxx		password;
	ffui_editxx		url;
	ffui_editxx		notes;
	ffui_editxx		tag;
	ffui_buttonxx	b_ok;
	ffui_buttonxx	copy_username;
	ffui_buttonxx	copy_passwd;
	ffui_paned		pn_notes;
} wentry;

#define _(m)  FFUI_LDR_CTL(wentry, m)
const ffui_ldr_ctl wentry_ctls[] = {
	_(wnd),
	_(title),
	_(username),
	_(password),
	_(url),
	_(notes),
	_(tag),
	_(b_ok),
	_(copy_username),
	_(copy_passwd),
	_(pn_notes),
	FFUI_LDR_CTL_END
};
#undef _

/** DB entry -> GUI */
void wentry_fill(const fpm_dbentry *ent)
{
	g->wentry->title.text(ent->title);
	g->wentry->username.text(ent->username);
	g->wentry->password.text(ent->passwd);
	g->wentry->url.text(ent->url);
	g->wentry->notes.text(ent->notes);

	if (ent->grp != -1) {
		const fpm_dbgroup *gr = dbif->grp(g->dbx, ent->grp);
		g->wentry->tag.text(gr->name);
	} else
		g->wentry->tag.text("");

	g->wentry->wnd.title("Edit Item");
	ShowWindow(g->wentry->wnd.h, SW_SHOWNOACTIVATE);
}

static void ent_copy(uint t)
{
	ffvecxx s;
	switch (t) {
	case H_USERNAME:
		s = g->wentry->username.text();  break;

	case H_PASSWD:
		s = g->wentry->password.text();  break;
	}
	ffui_clipbd_set((char*)s.ptr, s.len);
	wmain_status("Copied to clipboard");
}

void wentry_clear()
{
	g->wentry->title.text("");
	g->wentry->username.text("");
	g->wentry->password.text("");
	g->wentry->url.text("");
	g->wentry->notes.text("");
	g->wentry->tag.text("");
	g->active_ent = NULL;
	g->active_item = -1;
}

/** GUI -> DB entry */
int wentry_set(fpm_dbentry *ent)
{
	ent->title = g->wentry->title.text();
	ent->username = g->wentry->username.text();
	ent->passwd = g->wentry->password.text();
	ent->url = g->wentry->url.text();
	ent->notes = g->wentry->notes.text();
	ffvecxx tag = g->wentry->tag.text();
	if (tag.len == 0)
		ent->grp = -1;
	else {
		fpm_dbgroup *gr;
		ent->grp = dbif->grp_find(g->dbx, (char*)tag.ptr, tag.len);

		if (ent->grp == -1) {
			fpm_dbgroup gr1;
			gr1.name = tag.str();
			ent->grp = dbif->grp_add(g->dbx, &gr1);
			if (ent->grp == -1)
				goto fail;
			gr = dbif->grp(g->dbx, ent->grp);
			tag.reset();

			wmain_grp_add(gr->name);
		}
	}

	return 0;

fail:
	ffstr_free(&ent->title);
	ffstr_free(&ent->username);
	ffstr_free(&ent->passwd);
	ffstr_free(&ent->url);
	ffstr_free(&ent->notes);
	return 1;
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
	ffui_wnd_setfront(&g->wentry->wnd);
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

			g->active_ent = ent_ins();
			break;
		}
		ent_modify();
		break;

	case ENT_COPYUSENAME:
		ent_copy(H_USERNAME);  break;

	case ENT_COPYPASSWD:
		ent_copy(H_PASSWD);  break;
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
