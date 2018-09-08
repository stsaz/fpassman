/** Database operations.
Copyright (c) 2018 Simon Zolin
*/

#include <fpassman.h>

#include <FF/array.h>
#include <FF/time.h>
#include <FF/list.h>


typedef struct dbentry {
	fpm_dbentry e;

	fflist_item sib;
} dbentry;

struct fpm_db {
	ffarr grps; //fpm_dbgroup[]
	fflist ents; //dbentry[]
};


// DB interface
static fpm_db* db_create(void);
static fpm_dbentry* db_ent(uint cmd, fpm_db *db, fpm_dbentry *ent);
static void db_fin(fpm_db *db);
static int db_find(fpm_dbentry *ent, const char *search, size_t len);
static fpm_dbgroup* db_grp(fpm_db *db, uint i);
static int db_grp_find(fpm_db *db, const char *name, size_t len);
static int db_grp_add(fpm_db *db, fpm_dbgroup *grp);
static int db_grp_del(fpm_db *db, uint i);
static const struct fpm_dbiface dbiface = {
	&db_create, &db_fin, &db_ent, &db_find,
	&db_grp, &db_grp_find, &db_grp_add, &db_grp_del
};
const struct fpm_dbiface *dbif = &dbiface;


static void dbentry_free(dbentry *e);


static fpm_db* db_create(void)
{
	fpm_db *db;
	if (NULL == (db = ffmem_new(fpm_db)))
		return NULL;
	fflist_init(&db->ents);
	return db;
}

static fpm_dbentry* db_ent(uint cmd, fpm_db *db, fpm_dbentry *ent)
{
	dbentry *e;
	switch (cmd) {
	case FPM_DB_INS:
		if (NULL == (e = ffmem_new(dbentry)))
			return NULL;
		e->e = *ent;
		fflist_ins(&db->ents, &e->sib);
		return &e->e;

	case FPM_DB_RM:
		e = FF_GETPTR(dbentry, e, ent);
		fflist_rm(&db->ents, &e->sib);
		dbentry_free(e);
		break;

	case FPM_DB_MOD: {
		fftime t;
		fftime_now(&t);
		ent->mtime = t.sec;
		break;
	}

	case FPM_DB_NEXT: {
		dbentry *prev = FF_GETPTR(dbentry, e, ent);
		fflist_item *it;
		it = (ent == NULL) ? db->ents.first : prev->sib.next;
		if (it == fflist_sentl(&db->ents))
			return NULL;
		e = FF_GETPTR(dbentry, sib, it);
		return &e->e;
	}

	}

	return NULL;
}

static void dbentry_free(dbentry *e)
{
	ffstr_free(&e->e.title);
	ffstr_free(&e->e.username);
	ffstr_free(&e->e.passwd);
	ffstr_free(&e->e.url);
	ffstr_free(&e->e.notes);
	ffmem_free(e);
}

static void dbgrp_fin(fpm_dbgroup *g)
{
	ffstr_free(&g->name);
}

static void db_fin(fpm_db *db)
{
	fpm_dbgroup *grp;

	FFLIST_ENUMSAFE(&db->ents, dbentry_free, dbentry, sib);
	fflist_init(&db->ents);

	FFARR_WALKT(&db->grps, grp, fpm_dbgroup) {
		dbgrp_fin(grp);
	}
	ffarr_free(&db->grps);
}

static int db_find(fpm_dbentry *ent, const char *search, size_t len)
{
	ffstr src, s;
	ffstr_set(&src, search, len);

	while (src.len != 0) {
		ffstr_nextval3(&src, &s, ' ');

		if (ffstr_ifind(&ent->title, s.ptr, s.len) >= 0
			|| ffstr_ifind(&ent->username, s.ptr, s.len) >= 0
			|| ffstr_ifind(&ent->url, s.ptr, s.len) >= 0
			|| ffstr_ifind(&ent->notes, s.ptr, s.len) >= 0)
		{}
		else
			return 0;
	}
	return 1;
}


static fpm_dbgroup* db_grp(fpm_db *db, uint i)
{
	if (i == db->grps.len)
		return NULL;
	fpm_dbgroup *g = (void*)db->grps.ptr;
	return &g[i];
}

static int db_grp_find(fpm_db *db, const char *name, size_t len)
{
	fpm_dbgroup *g;
	FFARR_WALKT(&db->grps, g, fpm_dbgroup) {
		if (ffstr_ieq(&g->name, name, len))
			return g - (fpm_dbgroup*)db->grps.ptr;
	}
	return -1;
}

static int db_grp_add(fpm_db *db, fpm_dbgroup *grp)
{
	fpm_dbgroup *g = ffarr_pushT(&db->grps, fpm_dbgroup);
	if (g == NULL)
		return -1;
	*g = *grp;
	return db->grps.len - 1;
}

static int db_grp_del(fpm_db *db, uint i)
{
	dbentry *ent;
	FFLIST_WALK(&db->ents, ent, sib) {
		if (ent->e.grp == i)
			ent->e.grp = -1;
		else if (ent->e.grp > (int)i)
			ent->e.grp--;
	}

	_ffarr_rm(&db->grps, i, 1, sizeof(fpm_dbgroup));
	return 0;
}
