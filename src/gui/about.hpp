/** fpassman
2018, Simon Zolin */

#define FPM_HOMEPAGE  "https://github.com/stsaz/fpassman"

typedef struct wabout {
	ffui_windowxx	wnd;
	ffui_icon		ico;
	ffui_labelxx	labout;
	ffui_labelxx	lurl;
} wabout;

const ffui_ldr_ctl wabout_ctls[] = {
	FFUI_LDR_CTL(wabout, wnd),
	FFUI_LDR_CTL(wabout, labout),
	FFUI_LDR_CTL(wabout, lurl),
	FFUI_LDR_CTL(wabout, ico),
	FFUI_LDR_CTL_END
};

static void wabout_action(ffui_window *wnd, int id)
{
	switch (id) {
	case ABOUT_OPEN_HOMEPAGE:
#ifdef FF_WIN
		ffui_shellexec(FPM_HOMEPAGE, SW_SHOWNORMAL);
#endif
		break;
	}
}

void wabout_show(int show)
{
	if (show) {
		xxstr_buf<1000> s;
		g->wabout->labout.text(s.zfmt(
			"fpassman v%s\n\n"
			"Fast password manager"
			, FPM_VER));

#ifdef FF_WIN
		g->wabout->lurl.text(FPM_HOMEPAGE);
#else
		s.zfmt("<a href=\"%s\">%s</a>"
			, FPM_HOMEPAGE, FPM_HOMEPAGE);
		g->wabout->lurl.markup(s.ptr);
#endif
	}
	g->wabout->wnd.show(show);
}

void wabout_init()
{
	g->wabout = ffmem_new(struct wabout);
	g->wabout->wnd.on_action = wabout_action;
	g->wabout->wnd.hide_on_close = 1;
}
