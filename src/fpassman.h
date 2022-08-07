/** fpassman interfaces.
Copyright (c) 2018 Simon Zolin
*/

#pragma once
#include <FFOS/string.h>
#include <FFOS/std.h>
typedef ffbyte byte;
typedef ffuint uint;


// CORE

#define FPM_VER_MAJOR  1
#define FPM_VER_MINOR  2
#define FPM_VER  "1.2"
#define FPM_HOMEPAGE  "https://github.com/stsaz/fpassman"

struct fpm_conf {
	ffstr loaddb;
	char *loaddb_z;
};

struct fpm_core {
	int (*setroot)(const char *argv0);
	char* (*getpath)(const char *name, size_t len);
	struct fpm_conf* (*conf)();
	int (*loadconf)(void);
};

FF_EXTERN const struct fpm_core* core_init();
FF_EXTERN void core_free();

#define errlog(fmt, ...) \
	ffstderr_fmt(fmt "\n", ##__VA_ARGS__)


// DB

typedef struct fpm_dbgroup {
	ffstr name;
} fpm_dbgroup;

typedef struct fpm_dbentry {
	uint id;
	ffstr title;
	ffstr username;
	ffstr passwd;
	ffstr url;
	ffstr notes;

	uint mtime;
	int grp;
} fpm_dbentry;

#define	FPM_DB_KEYLEN 32

typedef struct fpm_db fpm_db;
struct fpm_dbf_iface {
	/** Get key from password string. */
	void (*keyinit)(byte *key, const char *passwd, size_t len);

	/** Open database file. */
	int (*open)(fpm_db *db, const char *fn, const byte *key);

	/** Save database to file. */
	int (*save)(fpm_db *db, const char *fn, const byte *key);
};

enum FPM_DB_CMD {
	FPM_DB_INS = 1,
	FPM_DB_RM,
	FPM_DB_MOD, //obsolete
	FPM_DB_NEXT,
	FPM_DB_SETBYID,
};

struct fpm_dbiface {
	fpm_db* (*create)(void);
	void (*fin)(fpm_db *db);

	/**
	cmd: enum FPM_DB_CMD. */
	fpm_dbentry* (*ent)(uint cmd, fpm_db *db, fpm_dbentry *ent);

	/** Match entry by words separated by space.
	Return 1 if matched. */
	int (*find)(fpm_dbentry *ent, const char *search, size_t len);

	fpm_dbgroup* (*grp)(fpm_db *db, uint i);

	/** Get group index by name.
	Return -1 if not found. */
	int (*grp_find)(fpm_db *db, const char *name, size_t len);

	int (*grp_add)(fpm_db *db, fpm_dbgroup *grp);
	int (*grp_del)(fpm_db *db, uint i);
};

extern const struct fpm_dbiface *dbif;
extern const struct fpm_dbf_iface *dbfif;
