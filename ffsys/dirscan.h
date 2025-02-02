/** ffsys: directory scan
2021, Simon Zolin */

/*
ffdirscan_open ffdirscan_close
ffdirscan_next
ffdirscan_reset
ffdirscan_index ffdirscan_count
ffdirscanx_open ffdirscanx_close
ffdirscanx_next
*/

#pragma once
#define _FFSYS_DIRSCAN_H

#include <ffsys/dir.h>
#include <ffsys/path.h>
#include <ffsys/file.h> // optional
#include <ffbase/vector.h>
#include <ffbase/stringz.h>

typedef struct ffdirscan {
	void *names; // NAME\0... uint...
	ffsize len, index, cur;

	const char *wildcard;
	fffd fd;
} ffdirscan;

#define ffdirscan_index(ds)  ((ffuint*)((char*)(ds)->names + (ds)->index))
#define ffdirscan_count(ds)  (((ds)->len - (ds)->index) / sizeof(ffuint))

enum FFDIRSCAN_F {
	FFDIRSCAN_NOSORT = 1, // don't sort
	FFDIRSCAN_DOT = 2, // include "." and ".."

	/** Filter names by wildcard */
	FFDIRSCAN_USEWILDCARD = 0x10,

	/** Linux: use fd supplied by user.
	Don't use this fd afterwards! */
	FFDIRSCAN_USEFD = 0x20,

	// FFDIRSCANX_SORT_REVERSE = 0x0100,
	FFDIRSCANX_SORT_DIRS = 0x0200,
};

/** Compare file names */
static int _ffdirscan_filename_cmpz(const char *a, const char *b)
{
	int first_diff = 0;
	for (ffsize i = 0;  ;  i++) {
		ffuint cl = (ffbyte)a[i];
		ffuint cr = (ffbyte)b[i];

		if (cl != cr) {
			if (cl >= 'A' && cl <= 'Z')
				cl |= 0x20;
			if (cr >= 'A' && cr <= 'Z')
				cr |= 0x20;
			if (cl < cr)
				return -1;
			else if (cl > cr)
				return 1;

			first_diff = (a[i] < b[i]) ? 1 : -1; // "a" < "A"

		} else if (cl == '\0') {
			return first_diff;
		}
	}
}

static int _ffdirscan_cmpname(const void *a, const void *b, void *udata)
{
	const char *buf = (char*)udata;
	ffuint off1 = *(ffuint*)a, off2 = *(ffuint*)b;

	if (FFPATH_ICASE)
		return ffsz_icmp(&buf[off1], &buf[off2]);

	return _ffdirscan_filename_cmpz(&buf[off1], &buf[off2]);
}

/**
flags: enum FFDIRSCAN_F
Default behaviour: skip "." and ".." entries, sort
Return 0 on success */
static inline int ffdirscan_open(ffdirscan *d, const char *path, ffuint flags)
{
	ffvec offsets = {}, buf = {};
	int rc = -1;

	ffuint wcflags;
	ffsize wclen = 0;
	if (flags & FFDIRSCAN_USEWILDCARD) {
		wclen = ffsz_len(d->wildcard);
		wcflags = FFPATH_ICASE ? FFS_WC_ICASE : 0;
	}

#ifdef FF_WIN

	if (path[0] == '\0') {
		SetLastError(ERROR_PATH_NOT_FOUND);
		return -1;
	}

	HANDLE dir = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATAW ent;
	wchar_t *wpath = NULL;
	char *namebuf = NULL;
	int first = 1;

	// "/dir" -> "/dir\\*"
	ffsize n = 3;
	if (flags & FFDIRSCAN_USEWILDCARD)
		n = 2+wclen;
	if (NULL == (wpath = ffs_alloc_utow_addcap(path, ffsz_len(path), &n)))
		goto end;
	if (wpath[n-1] == '/' || wpath[n-1] == '\\')
		n--; // "dir/" -> "dir"
	wpath[n++] = '\\';
	if (flags & FFDIRSCAN_USEWILDCARD)
		n += _ffs_utow(&wpath[n], wclen, d->wildcard, wclen);
	else
		wpath[n++] = '*';
	wpath[n] = '\0';

	if (flags & FFDIRSCAN_USEFD)
		SetLastError(ERROR_NOT_SUPPORTED);
	else {
		dir = FindFirstFileW(wpath, &ent);
		if (dir == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_NOT_FOUND)
			goto end;
	}
	ffmem_free(wpath);  wpath = NULL;

	if (NULL == (namebuf = (char*)ffmem_alloc(256*4)))
		goto end;

#else

	DIR *dir = NULL;
	if (flags & FFDIRSCAN_USEFD) {
		dir = fdopendir(d->fd);
		if (dir == NULL)
			close(d->fd);
		d->fd = -1;
	} else {
		dir = opendir(path);
	}

	if (dir == NULL)
		goto end;
#endif

	if (NULL == ffvec_alloc(&buf, 4096, 1))
		goto end;

	for (;;) {

		const char *name;

#ifdef FF_WIN

		if (first) {
			first = 0;
			if (dir == INVALID_HANDLE_VALUE)
				break;

		} else {
			if (0 == FindNextFileW(dir, &ent)) {
				if (GetLastError() != ERROR_NO_MORE_FILES)
					goto end;
				break;
			}
		}

		if (ffsz_wtou(namebuf, 256*4, ent.cFileName) < 0) {
			SetLastError(ERROR_INVALID_DATA);
			goto end;
		}
		name = namebuf;

#else

		const struct dirent *de;
		errno = 0;
		if (NULL == (de = readdir(dir))) {
			if (errno != 0)
				goto end;
			break;
		}
		name = de->d_name;
#endif

		ffsize namelen = ffsz_len(name);

		if (!(flags & FFDIRSCAN_DOT)) {
			if (name[0] == '.'
				&& (name[1] == '\0'
					|| (name[1] == '.' && name[2] == '\0')))
				continue;
		}

		if (flags & FFDIRSCAN_USEWILDCARD) {
			if (0 != ffs_wildcard(d->wildcard, wclen, name, namelen, wcflags))
				continue;
		}

		ffuint *p = ffvec_pushT(&offsets, ffuint);
		if (p == NULL)
			goto end;
		*p = buf.len;

		if (0 == ffvec_add(&buf, name, namelen+1, 1))
			goto end;
	}

	if (!(flags & FFDIRSCAN_NOSORT))
		ffsort(offsets.ptr, offsets.len, sizeof(ffuint), _ffdirscan_cmpname, buf.ptr);

	d->index = buf.len;
	if (offsets.len != 0 && 0 == ffvec_add(&buf, offsets.ptr, offsets.len * sizeof(ffuint), 1))
		goto end;
	d->len = buf.len;
	d->cur = d->index;
	d->names = buf.ptr;
	ffvec_null(&buf);
	rc = 0;

end:
#ifdef FF_WIN
	if (dir != INVALID_HANDLE_VALUE)
		FindClose(dir);
	ffmem_free(wpath);
	ffmem_free(namebuf);
#else
	if (dir != NULL)
		closedir(dir);
#endif
	ffvec_free(&offsets);
	ffvec_free(&buf);
	return rc;
}

static inline void ffdirscan_close(ffdirscan *d)
{
	ffmem_free(d->names);  d->names = NULL;
}

/**
Return NULL if no more entries */
static inline const char* ffdirscan_next(ffdirscan *d)
{
	if (d->cur == d->len)
		return NULL;

	ffsize off = *(ffuint*)&((char*)d->names)[d->cur];
	d->cur += sizeof(ffuint);
	const char *name = &((char*)d->names)[off];
	return name;
}

static inline void ffdirscan_reset(ffdirscan *d)
{
	d->cur = d->index;
}


#ifdef _FFSYS_FILE_H

/** Extended directory scanner. */
typedef struct ffdirscanx {
	ffdirscan ds;
	ffuint flags;
} ffdirscanx;

/** Sort file names (directories first). */
static int _ffdsx_open_cmp(const void *a, const void *b, void *udata)
{
	const ffdirscanx *dx = (ffdirscanx*)udata;
	ffuint l = *(ffuint*)a, r = *(ffuint*)b;
	if (!(dx->flags & FFDIRSCANX_SORT_DIRS)
		|| (l & 0x80000000) == (r & 0x80000000)) {
		return _ffdirscan_filename_cmpz((char*)dx->ds.names + (l & ~0x80000000), (char*)dx->ds.names + (r & ~0x80000000));
	}

	return (l & 0x80000000) ? -1 : 1;
}

static inline void ffdirscanx_close(ffdirscanx *dx)
{
	ffdirscan_close(&dx->ds);
}

/** Scan directory and fetch file info. */
static inline int ffdirscanx_open(ffdirscanx *dx, const char *path, ffuint flags)
{
	char *s = NULL, *s_name;
	int rc = 1;
	ffuint i = 0;
	ffstr s_path = FFSTR_INITZ(path);

	if (ffdirscan_open(&dx->ds, path, flags | FFDIRSCAN_NOSORT))
		goto end;

	if (!(s = (char*)ffmem_alloc(s_path.len + 1 + 255*4)))
		goto end;
	ffmem_copy(s, s_path.ptr, s_path.len);
	s[s_path.len] = FFPATH_SLASH;
	s_name = s + s_path.len + 1;

	const char *fn;
	while ((fn = ffdirscan_next(&dx->ds))) {
		ffsz_copyz(s_name, 255*4, fn);

		fffileinfo fi;
		if (!fffile_info_path(s, &fi)) {
			ffuint *off = (ffuint*)((char*)dx->ds.names + dx->ds.cur - sizeof(ffuint));
			FF_ASSERT(0 == (*off & 0x80000000));
			if (fffile_isdir(fffileinfo_attr(&fi)))
				*off |= 0x80000000;
		}
		i++;
	}
	dx->ds.cur = dx->ds.index;

	dx->flags = flags;
	ffsort(ffdirscan_index(&dx->ds), ffdirscan_count(&dx->ds), sizeof(int), _ffdsx_open_cmp, dx);
	rc = 0;

end:
	if (rc)
		ffdirscanx_close(dx);
	ffmem_free(s);
	return rc;
}

static inline const char* ffdirscanx_next(ffdirscanx *dx)
{
	if (dx->ds.cur == dx->ds.len)
		return NULL;

	ffuint off = *(ffuint*)((char*)dx->ds.names + dx->ds.cur);
	dx->ds.cur += sizeof(ffuint);
	const char *name = (char*)dx->ds.names + (off & ~0x80000000);
	return name;
}

#endif // _FFSYS_FILE_H
