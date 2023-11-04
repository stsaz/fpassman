/** fpassman: GUI
2018, Simon Zolin */

#include <util/windows-shell.h>
#include <gui/gui.hpp>
#include <ffgui/winapi/loader.h>
#include <ffbase/time.h>
#include <ffsys/file.h>
#include <ffsys/globals.h>

const struct fpm_core *core;
gui *g;

#include "main.hpp"
#include "db.hpp"
#include "entry.hpp"
#include "find.hpp"
#include "about.hpp"

static const ffui_ldr_ctl top_ctls[] = {
	FFUI_LDR_CTL(gui, dlg),
	FFUI_LDR_CTL(gui, mfile),
	FFUI_LDR_CTL(gui, mgroup),
	FFUI_LDR_CTL(gui, mentry),
	FFUI_LDR_CTL(gui, mhelp),
	FFUI_LDR_CTL(gui, mtray),

	FFUI_LDR_CTL3_PTR(gui, wmain, wmain_ctls),
	FFUI_LDR_CTL3_PTR(gui, wentry, wentry_ctls),
	FFUI_LDR_CTL3_PTR(gui, wfind, wfind_ctls),
	FFUI_LDR_CTL3_PTR(gui, wdb, wdb_ctls),
	FFUI_LDR_CTL3_PTR(gui, wabout, wabout_ctls),
	FFUI_LDR_CTL_END
};

#define A(id)  #id
static const char *const scmds[] = {
	#include "actions.h"
};
#undef A

static void* getctl(void *udata, const ffstr *name)
{
	gui *g = (gui*)udata;
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

static int gui_run()
{
	ffui_loader ldr;
	int r;
	char *fn;

	ffui_init();
	ffui_wnd_initstyle();

	wmain_init();
	wdb_init();
	wentry_init();
	wfind_init();
	wabout_init();

	ffui_ldr_init(&ldr, getctl, getcmd, g);
	ldr.hmod_resource = GetModuleHandleW(NULL);

	if (NULL == (fn = core->getpath(FFSTR_Z("fpassman.gui")))) {
		r = 1;
		goto done;
	}
	r = ffui_ldr_loadfile(&ldr, fn);
	ffmem_free(fn);

	if (r != 0) {
		ffvec msg = {};
		ffvec_addfmt(&msg, "fpassman.gui: %s\n", ffui_ldr_errstr(&ldr));
		fffile_write(ffstderr, msg.ptr, msg.len);
		ffui_msgdlg_show("fpassman", (char*)msg.ptr, msg.len, FFUI_MSGDLG_ERR);
		ffvec_free(&msg);
		ffui_ldr_fin(&ldr);
		goto done;
	}
	ffui_ldr_fin(&ldr);

	wmain_show();

	if (core->conf()->loaddb.len != 0) {
		wdb_open(1);
	}

	ffui_run();
	r = 0;

done:
	ffui_dlg_destroy(&g->dlg);
	ffui_uninit();
	return r;
}

static void cleanup()
{
	if (g->dbx)
		dbif->fin(g->dbx);
	if (core)
		core_free();
}

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	int r = 1;
	g = ffmem_new(gui);
	if (!(core = core_init())) goto done;
	if (core->setroot(NULL)) goto done;
	if (core->loadconf()) goto done;
	if (!(g->dbx = dbif->create())) goto done;
	if (gui_run()) goto done;
	r = 0;

done:
	cleanup();
	ffmem_free(g);  g = NULL;
	return r;
}
