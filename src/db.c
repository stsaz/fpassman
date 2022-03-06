/** Database operations.
Copyright (c) 2018 Simon Zolin
*/

#include <fpassman.h>
#include <ffbase/vector.h>
#include <ffbase/list.h>
#include <FFOS/time.h>
#include <FFOS/error.h>


typedef struct dbentry {
	fpm_dbentry e;

	ffchain_item sib;
} dbentry;

struct fpm_db {
	ffvec grps; //fpm_dbgroup[]
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
		fflist_add(&db->ents, &e->sib);
		return &e->e;

	case FPM_DB_RM:
		e = FF_STRUCTPTR(dbentry, e, ent);
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
		dbentry *prev = FF_STRUCTPTR(dbentry, e, ent);
		ffchain_item *it;
		it = (ent == NULL) ? fflist_first(&db->ents) : prev->sib.next;
		if (it == fflist_sentl(&db->ents))
			return NULL;
		e = FF_STRUCTPTR(dbentry, sib, it);
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

/** Call func() for every item in list.
A list item pointer is translated (by offset in a structure) into an object.
Example:
fflist mylist;
struct mystruct_t {
	...
	ffchain_item sibling;
};
FFLIST_ENUMSAFE(&mylist, ffmem_free, struct mystruct_t, sibling); */
#define FFLIST_ENUMSAFE(lst, func, struct_name, member_name) \
do { \
	ffchain_item *li; \
	for (li = (lst)->root.next;  li != fflist_sentl(lst); ) { \
		void *p = FF_STRUCTPTR(struct_name, member_name, li); \
		li = li->next; \
		func(p); \
	} \
} while (0)

static void db_fin(fpm_db *db)
{
	fpm_dbgroup *grp;

	FFLIST_ENUMSAFE(&db->ents, dbentry_free, dbentry, sib);
	fflist_init(&db->ents);

	FFSLICE_WALK(&db->grps, grp) {
		dbgrp_fin(grp);
	}
	ffvec_free(&db->grps);
}

static int db_find(fpm_dbentry *ent, const char *search, size_t len)
{
	ffstr src, s;
	ffstr_set(&src, search, len);

	while (src.len != 0) {
		ffstr_splitby(&src, ' ', &s, &src);

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
	FFSLICE_WALK(&db->grps, g) {
		if (ffstr_ieq(&g->name, name, len))
			return g - (fpm_dbgroup*)db->grps.ptr;
	}
	return -1;
}

static int db_grp_add(fpm_db *db, fpm_dbgroup *grp)
{
	fpm_dbgroup *g = ffvec_pushT(&db->grps, fpm_dbgroup);
	if (g == NULL)
		return -1;
	*g = *grp;
	return db->grps.len - 1;
}

static int db_grp_del(fpm_db *db, uint i)
{
	ffchain_item *it;
	FFLIST_WALK(&db->ents, it) {
		dbentry *ent = FF_STRUCTPTR(dbentry, sib, it);
		if (ent->e.grp == i)
			ent->e.grp = -1;
		else if (ent->e.grp > (int)i)
			ent->e.grp--;
	}

	ffslice_rm((ffslice*)&db->grps, i, 1, sizeof(fpm_dbgroup));
	return 0;
}
