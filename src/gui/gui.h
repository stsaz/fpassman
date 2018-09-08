/**
Copyright (c) 2018 Simon Zolin
*/

#include <fpassman.h>

#include <FF/gui/winapi.h>


typedef struct wmain {
	ffui_wnd wmain;
	ffui_menu mm;
	ffui_view grps;
	ffui_view area;
	ffui_stbar stbar;
	ffui_paned pn;
	ffui_trayicon trayicon;
} wmain;

typedef struct wentry {
	ffui_wnd wentry;
	ffui_edit eEntTitle;
	ffui_edit eEntUsername;
	ffui_edit eEntPassword;
	ffui_edit eEntUrl;
	ffui_edit eEntNotes;
	ffui_edit eEntTag;
	ffui_btn bEntOk;
	ffui_btn bEntCopyUsername;
	ffui_btn bEntCopyPasswd;
	ffui_paned pn_notes;
} wentry;

typedef struct wfind {
	ffui_wnd wfind;
	ffui_edit efind;
} wfind;

typedef struct wdb {
	ffui_wnd wdb;
	ffui_edit edbfn;
	ffui_edit edbpass, edbpass2;
	ffui_btn bbrowse;
	ffui_btn bdbdo;
} wdb;

typedef struct wabout {
	ffui_wnd wabout;
	ffui_icon ico;
	ffui_label labout;
	ffui_label lurl;
} wabout;

typedef struct gui {
	ffui_dialog dlg;

	ffui_menu mfile, mgroup, mentry, mhelp;
	ffui_menu mtray;

	wmain wmain;

	fpm_dbentry *active_ent;
	int active_item;

	wentry wentry;
	uint entry_creating :1;

	wfind wfind;

	wdb wdb;
	uint db_op;
	char *saved_fn;
	byte saved_key[32];
	uint saved_key_valid :1;

	wabout wabout;

	fpm_db *dbx;
} gui;

extern gui *g;


enum CMDS {
	PM_QUIT = 1,
	PM_ABOUT,
	PM_SHOW,
	PM_MINTOTRAY,
	GRP_SEL,
	AREA_ACTION,
	AREA_CHSEL,
	OPEN_HOMEPAGE,

	DB_OPEN,
	DB_SAVE,
	DB_SAVEAS,
	DB_DO,
	DB_BROWSE,
	DB_CLOSE,

	GRP_DEL,

	ENT_NEW,
	ENT_EDIT,
	ENT_DEL,
	ENT_FIND,
	ENT_FINDSHOW,
	ENT_DONE,
	ENT_COPYUSENAME,
	ENT_COPYPASSWD,
	ENT_CANCEL,
};
