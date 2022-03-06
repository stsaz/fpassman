// JSON scheme

/* File format:
{
ver:1,

groups:[
	[name]
],

entries:[
	[title, username, password, url, notes, mtimeINT, groupID]
]
}
*/

enum {
	//0..4
	I_MTIME = 5,
	I_GROUP,
};

#define _F(x)  (ffsize)x

static int grp_item(ffjson_scheme *js, dbparse *dbp, const ffstr *val)
{
	if (dbp->idx != 0)
		return JSON_EBADVAL;
	dbp->idx++;
	ffjson_strval_acquire(js->parser, &dbp->grp.name);
	return 0;
}
static int grp_done(ffjson_scheme *js, dbparse *dbp)
{
	dbif->grp_add(dbp->db, &dbp->grp);
	return 0;
}
static const ffjson_arg grp_args[] = {
	{ "*", FFJSON_TSTR, _F(grp_item) },
	{ NULL, FFJSON_TARR_CLOSE, _F(grp_done) },
};

static int new_grp(ffjson_scheme *js, dbparse *dbp)
{
	ffmem_zero_obj(&dbp->grp);
	ffjson_scheme_addctx(js, grp_args, dbp);
	dbp->idx = 0;
	return 0;
}
static const ffjson_arg grps_args[] = {
	{ "*", FFJSON_TARR_OPEN, _F(new_grp) },
	{}
};

static int ent_item_int(ffjson_scheme *js, dbparse *dbp, int64 val)
{
	fpm_dbentry *ent = &dbp->ent;
	switch (dbp->idx) {
	case I_MTIME:
		if ((uint)val != val)
			return FFJSON_EBADINT;
		ent->mtime = (uint)val;
		break;

	case I_GROUP:
		if (val == -1) {
			dbp->idx++;
			return 0;
		}
		ent->grp = (int)val;
		break;

	default:
		return FFJSON_EBADINT;
	}

	dbp->idx++;
	return 0;
}
static int ent_item(ffjson_scheme *js, dbparse *dbp)
{
	if (js->jtype == FFJSON_TINT)
		return ent_item_int(js, dbp, js->parser->intval);
	else if (js->jtype != FFJSON_TSTR)
		return JSON_EBADVAL;

	fpm_dbentry *ent = &dbp->ent;
	switch (dbp->idx) {
	case 0:
		ffjson_strval_acquire(js->parser, &ent->title);
		break;
	case 1:
		ffjson_strval_acquire(js->parser, &ent->username);
		break;
	case 2:
		ffjson_strval_acquire(js->parser, &ent->passwd);
		break;
	case 3:
		ffjson_strval_acquire(js->parser, &ent->url);
		break;
	case 4:
		ffjson_strval_acquire(js->parser, &ent->notes);
		break;

	default:
		return JSON_EBADVAL;
	}

	dbp->idx++;
	return 0;
}
static int ent_done(ffjson_scheme *js, dbparse *dbp)
{
	dbif->ent(FPM_DB_INS, dbp->db, &dbp->ent);
	return 0;
}
static const ffjson_arg ent_args[] = {
	{ "*", FFJSON_TANY, _F(ent_item) },
	{ NULL, FFJSON_TARR_CLOSE, _F(ent_done) },
};

static int new_ent(ffjson_scheme *js, dbparse *dbp)
{
	ffmem_zero_obj(&dbp->ent);
	ffjson_scheme_addctx(js, ent_args, dbp);
	dbp->idx = 0;
	dbp->ent.grp = -1;
	return 0;
}
static const ffjson_arg ents_args[] = {
	{ "*", FFJSON_TARR_OPEN, _F(new_ent) },
	{}
};

static int dbf_ver(ffjson_scheme *js, dbparse *dbp, int64 val)
{
	if (val != 1)
		return JSON_EBADVAL;
	dbp->ver = 1;
	return 0;
}
static int dbf_groups(ffjson_scheme *js, dbparse *dbp)
{
	ffjson_scheme_addctx(js, grps_args, dbp);
	return 0;
}
static int dbf_ents(ffjson_scheme *js, dbparse *dbp)
{
	ffjson_scheme_addctx(js, ents_args, dbp);
	return 0;
}
static const ffjson_arg dbf_args[] = {
	{ "ver", FFJSON_TINT, _F(dbf_ver) },
	{ "groups", FFJSON_TARR_OPEN, _F(dbf_groups) },
	{ "entries", FFJSON_TARR_OPEN, _F(dbf_ents) },
	{}
};


void json_prep(fpm_db *db, ffstr *dst)
{
	ffjsonw jw = {};
	ffjsonw_init(&jw);

	ffjsonw_addobj(&jw, 1);

		ffjsonw_addkeyz(&jw, "ver");
		ffjsonw_addint(&jw, 1);

		ffjsonw_addkeyz(&jw, "groups");
		ffjsonw_addarr(&jw, 1);

			fpm_dbgroup *grp;
			for (size_t i = 0;  NULL != (grp = dbif->grp(db, i));  i++) {
				ffjsonw_addarr(&jw, 1);
					ffjsonw_addstr(&jw, &grp->name);
				ffjsonw_addarr(&jw, 0);
			}

		ffjsonw_addarr(&jw, 0);

		ffjsonw_addkeyz(&jw, "entries");
		ffjsonw_addarr(&jw, 1);

			fpm_dbentry *ent;
			for (ent = NULL;  NULL != (ent = dbif->ent(FPM_DB_NEXT, db, ent));) {
				ffjsonw_addarr(&jw, 1);
					ffjsonw_addstr(&jw, &ent->title);
					ffjsonw_addstr(&jw, &ent->username);
					ffjsonw_addstr(&jw, &ent->passwd);
					ffjsonw_addstr(&jw, &ent->url);
					ffjsonw_addstr(&jw, &ent->notes);
					ffjsonw_addint(&jw, ent->mtime);
					ffjsonw_addint(&jw, ent->grp);
				ffjsonw_addarr(&jw, 0);
			}

		ffjsonw_addarr(&jw, 0);
	ffjsonw_addobj(&jw, 0);

	ffstr_setstr(dst, &jw.buf);
	ffvec_null(&jw.buf);
	ffjsonw_close(&jw);
}
