/** fpassman
2018, Simon Zolin */

typedef struct wdb {
	ffui_windowxx	wnd;
	ffui_editxx		edbfn;
	ffui_editxx		edbpass, edbpass2;
	ffui_buttonxx	bbrowse;
	ffui_buttonxx	bdbdo;
} wdb;

#define _(m)  FFUI_LDR_CTL(wdb, m)
const ffui_ldr_ctl wdb_ctls[] = {
	_(wnd),
	_(edbfn),
	_(edbpass),
	_(edbpass2),
	_(bbrowse),
	_(bdbdo),
	FFUI_LDR_CTL_END
};
#undef _

enum {
	WDB_OPLOAD = 1,
	WDB_OPSAVE,
};

static void wdb_load(void)
{
	int r;
	byte key[FPM_DB_KEYLEN];

	wdb_close();
	ffvecxx fn = g->wdb->edbfn.text();

	ffstr qpass = g->wdb->edbpass.text();
	g->wdb->edbpass.text("");
	dbfif->keyinit(key, qpass.ptr, qpass.len);
	priv_clear(qpass);
	ffstr_free(&qpass);

	wmain_status("Loading database...");

	r = dbfif->open(g->dbx, (char*)fn.ptr, key);
	if (r != 0) {
		char buf[1024];
		ffs_format_r0(buf, FF_COUNT(buf), "Error: %e: %E%Z", r, fferr_last());
		wmain_status(buf);
		return;
	}
	g->wdb->wnd.show(0);

	g->saved_fn = (char*)fn.ptr;
	fn.reset();
	memcpy(g->saved_key, key, sizeof(key));
	g->saved_key_valid = 1;

	area_fill();

	prep_tree();
	grps_fill();
}

static void wdb_save()
{
	ffvecxx pass = g->wdb->edbpass.text()
		, pass2 = g->wdb->edbpass2.text();

	if (pass.len == 0 || !ffstr_eq2(&pass, &pass2)) {
		wmain_status("Error: The passwords must match");
		return;
	}

	g->wdb->edbpass.text("");
	g->wdb->edbpass2.text("");

	byte key[FPM_DB_KEYLEN];
	dbfif->keyinit(key, (char*)pass.ptr, pass.len);
	priv_clear(pass.str());
	pass.free();
	priv_clear(pass2.str());
	pass2.free();

	if (0 != dosave(ffvecxx(g->wdb->edbfn.text()).str().ptr, key))
		return;

	g->wdb->wnd.show(0);
}

static void wdb_browse(void)
{
	const char *fn = "";

	if (g->db_op == WDB_OPLOAD) {
		ffui_dlg_titlez(&g->dlg, "Open database");
		fn = ffui_dlg_open(&g->dlg, &g->wdb->wnd);

	} else {
		ffui_dlg_titlez(&g->dlg, "Save database");
		fn = ffui_dlg_save(&g->dlg, &g->wdb->wnd, NULL, 0);
	}

	if (fn == NULL)
		return;

	g->wdb->edbfn.text(fn);
}

static void wdb_action(ffui_wnd *wnd, int id)
{
	switch (id) {
	case DB_BROWSE:
		wdb_browse();  break;

	case DB_DO:
		if (g->db_op == WDB_OPLOAD) {
			wdb_load();
		} else if (g->db_op == WDB_OPSAVE) {
			wdb_save();
		}
		break;
	}
}

void wdb_open(uint flags)
{
	g->db_op = WDB_OPLOAD;
	g->wdb->wnd.title("Open database");
	g->wdb->bdbdo.text("Open");
	ffui_show(&g->wdb->edbpass2, 0);
	g->wdb->edbfn.focus();
	g->wdb->wnd.show(1);
	ffui_wnd_setfront(&g->wdb->wnd);

	if (flags) {
		ffui_settextstr(&g->wdb->edbfn, &core->conf()->loaddb);
		g->wdb->edbpass.focus();
	}
}

void wdb_saveas()
{
	g->db_op = WDB_OPSAVE;
	g->wdb->wnd.title("Save database");
	g->wdb->bdbdo.text("Save");
	ffui_show(&g->wdb->edbpass2, 1);
	g->wdb->edbfn.focus();
	g->wdb->wnd.show(1);
	ffui_wnd_setfront(&g->wdb->wnd);
}

void wdb_show(uint show)
{
	g->wdb->wnd.show(show);
}

void wdb_init()
{
	g->wdb = ffmem_new(struct wdb);
	g->wdb->wnd.on_action = &wdb_action;
	g->wdb->wnd.hide_on_close = 1;
}
