/** fpassman
2018, Simon Zolin */

typedef struct wfind {
	ffui_windowxx	wnd;
	ffui_editxx		efind;
} wfind;

const ffui_ldr_ctl wfind_ctls[] = {
	FFUI_LDR_CTL(wfind, wnd),
	FFUI_LDR_CTL(wfind, efind),
	FFUI_LDR_CTL_END
};

static void wfind_action(ffui_wnd *wnd, int id)
{
	switch (id) {
	case ENT_FIND:
		wmain_filter(ffvecxx(g->wfind->efind.text()).str());  break;
	}
}

void wfind_clear()
{
	g->wfind->efind.text("");
}

void wfind_show(uint show)
{
	if (show)
		g->wfind->efind.focus();
	g->wfind->wnd.show(show);
}

void wfind_init()
{
	g->wfind = ffmem_new(struct wfind);
	g->wfind->wnd.on_action = wfind_action;
	g->wfind->wnd.hide_on_close = 1;
}
