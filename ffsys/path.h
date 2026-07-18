/** ffsys: file-system path
2020, Simon Zolin */

/*
FFDIR_USER_CONFIG FFPATH_ICASE FFPATH_SLASH
ffpath_slash
ffpath_isroot
ffpath_abs
ffpath_islong
ffpath_infoinit
ffpath_info
ffpath_splitpath ffpath_splitpath_str ffpath_splitpath_win ffpath_splitpath_unix
ffpath_splitname
ffpath_normalize
ffpath_norm
*/

#pragma once
#include <ffsys/string.h>

enum FFPATH_INFO {
	FFPATH_BLOCKSIZE,
};

#ifdef FF_WIN

#define FFDIR_USER_CONFIG  "%APPDATA%"

/** Case-insensitive file system */
#define FFPATH_ICASE  1

#define FFPATH_SLASH  '\\'
#define FFPATH_SLASHES  "\\/"

static inline ffbool ffpath_slash(int c)
{
	return c == '/' || c == '\\';
}

static inline int ffpath_isroot(const char *path, ffsize len)
{
	return ((len == 1 && (path[0] == '\\' || path[0] == '/'))
		|| (len == 2 && ((path[0] | 0x20) >= 'a' && (path[0] | 0x20) <= 'z'
			&& path[1] == ':'))
		|| (len == 3 && ((path[0] | 0x20) >= 'a' && (path[0] | 0x20) <= 'z'
			&& path[1] == ':'
			&& (path[2] == '\\' || path[2] == '/'))));
}

/* [/\\] || [a-zA-Z]:[\0/\\] */
static inline ffbool ffpath_abs(const char *path, ffsize len)
{
	if (len == 0)
		return 0;

	if (path[0] == '/' || path[0] == '\\')
		return 1;

	int ch = path[0] | 0x20;
	return len >= FFS_LEN("c:")
		&& ch >= 'a' && ch <= 'z'
		&& path[1] == ':'
		&& (len == FFS_LEN("c:") || path[2] == '/' || path[2] == '\\');
}

/**
Return TRUE if long name */
static inline ffbool ffpath_islong(const wchar_t *fnw)
{
	wchar_t fnlong[4096];
	ffuint n = GetLongPathNameW(fnw, fnlong, FF_COUNT(fnlong));
	if (n == 0)
		return 0;
	if (0 != _wcsicmp(fnlong, fnw))
		return 0;
	return 1;
}


typedef struct ffpathinfo {
	ffuint f_bsize;
} ffpathinfo;

static inline int ffpath_infoinit(const char *path, ffpathinfo *st)
{
	wchar_t *w, ws[256];
	if (NULL == (w = ffsz_alloc_buf_utow(ws, FF_COUNT(ws), path)))
		return 1;

	DWORD spc, bps, fc, c;
	BOOL b = GetDiskFreeSpaceW(w, &spc, &bps, &fc, &c);

	if (w != ws)
		ffmem_free(w);
	if (!b)
		return 1;

	st->f_bsize = spc * bps;
	return 0;
}

static inline ffuint64 ffpath_info(ffpathinfo *st, ffuint name)
{
	switch (name) {
	case FFPATH_BLOCKSIZE:
		return st->f_bsize;
	}

	return 0;
}

#define ffpath_splitpath ffpath_splitpath_win

#else // UNIX:

#if defined FF_BSD || defined FF_APPLE
	#include <sys/mount.h>
#else
	#include <sys/vfs.h>
#endif

#define FFDIR_USER_CONFIG  "$HOME/.config"

/** Case-sensitive file system */
#define FFPATH_ICASE  0

/** File system native path separator */
#define FFPATH_SLASH  '/'
#define FFPATH_SLASHES  "/"

static inline ffbool ffpath_slash(int c)
{
	return c == '/';
}

static inline int ffpath_isroot(const char *path, ffsize len)
{
	return len == 1 && path[0] == '/';
}

static inline ffbool ffpath_abs(const char *path, ffsize len)
{
	return len >= 1 && path[0] == '/';
}


typedef struct statfs ffpathinfo;

static inline int ffpath_infoinit(const char *path, ffpathinfo *st)
{
	return statfs(path, st);
}

static inline ffuint64 ffpath_info(ffpathinfo *st, ffuint name)
{
	switch (name) {
	case FFPATH_BLOCKSIZE:
		return st->f_bsize;
	}

	return 0;
}

#define ffpath_splitpath ffpath_splitpath_unix

#endif


/** Return TRUE if character is a valid path separator */
static ffbool ffpath_slash(int c);

/** Return 1 if path is the root directory.
Linux: "/"
Windows: "c:\" or "\"
*/
static int ffpath_isroot(const char *path, ffsize len);

/** Check whether the specified path is absolute */
static ffbool ffpath_abs(const char *path, ffsize len);


/** Initialize statfs structure */
static int ffpath_infoinit(const char *path, ffpathinfo *st);

/** Get file-system parameter
name: enum FFPATH_INFO */
static ffuint64 ffpath_info(ffpathinfo *st, ffuint name);

/** Split into path (without the last slash) and name so that "foo" is a name without path */
static inline ffssize ffpath_splitpath_unix(const char *fn, ffsize len, ffstr *dir, ffstr *name)
{
	ffssize slash = ffs_rfindchar(fn, len, '/');
	if (slash < 0) {
		if (dir != NULL)
			ffstr_null(dir);
		if (name != NULL)
			ffstr_set(name, fn, len);
		return -1;
	}
	return ffs_split(fn, len, slash, dir, name);
}

static inline ffssize ffpath_splitpath_win(const char *fn, ffsize len, ffstr *dir, ffstr *name)
{
	ffssize slash = ffs_rfindany(fn, len, "/\\", 2);
	if (slash < 0) {
		if (dir != NULL)
			ffstr_null(dir);
		if (name != NULL)
			ffstr_set(name, fn, len);
		return -1;
	}
	return ffs_split(fn, len, slash, dir, name);
}

/** Split into name and extension so that ".foo" is a name without extension */
static inline ffssize ffpath_splitname(const char *fn, ffsize len, ffstr *name, ffstr *ext)
{
	ffssize dot = ffs_rfindchar(fn, len, '.');
	if (dot <= 0) {
		if (name != NULL)
			ffstr_set(name, fn, len);
		if (ext != NULL)
			ext->len = 0;
		return dot;
	}
	return ffs_split(fn, len, dot, name, ext);
}

static inline ffssize ffpath_splitpath_str(ffstr fn, ffstr *dir, ffstr *name)
{
	return ffpath_splitpath(fn.ptr, fn.len, dir, name);
}

static inline ffssize ffpath_splitname_str(ffstr fn, ffstr *name, ffstr *ext)
{
	return ffpath_splitname(fn.ptr, fn.len, name, ext);
}

enum FFPATH_NORM_V1 {
	FFPATH_SLASH_ONLY = 1, // split path by slash (default on UNIX)
	FFPATH_SLASH_BACKSLASH = 2, // split path by both slash and backslash (default on Windows)
	FFPATH_FORCE_SLASH = 4, // convert '\\' to '/'
	FFPATH_FORCE_BACKSLASH = 8, // convert '/' to '\\'

	FFPATH_SIMPLE = 0x10, // convert to a simple path: {/abc, ./abc, ../abc} -> abc

	/* Handle disk drive letter, e.g. 'C:\' (default on Windows)
	Skip all paths starting with double backslash ("\\").
	C:/../a -> C:/a
	C:/a -> a (FFPATH_SIMPLE) */
	FFPATH_DISK_LETTER = 0x20,
	FFPATH_NO_DISK_LETTER = 0x40, // disable auto FFPATH_DISK_LETTER on Windows
};

/** Normalize file path
flags: enum FFPATH_NORM_V1

Windows-specific path examples:
* "c:\dir\file.txt" (DOS path)
* "\\.\c:\dir\file.txt" (device path)
* "\\?\c:\dir\file.txt" (device path)
* "\\.\Volume{00000000-0000-0000-0000-000000000000}\dir\file.txt" (device path)
* "\\localhost\c$\dir\file.txt" (UNC path)

Default behaviour:
* Split path by slash (on UNIX), by both slash and backslash (on Windows).
	Override by FFPATH_SLASH_BACKSLASH or FFPATH_SLASH_ONLY.
* Handle disk drive letter and complex paths (on Windows), e.g. "C:\" or "\\.\".
	Override by FFPATH_DISK_LETTER or FFPATH_NO_DISK_LETTER.
* Skip "." components, unless leading:
	./a/b -> ./a/b
	a/./b -> a/b
* Handle ".." components:
	a/../b -> b
	../a/b -> ../a/b
	./../a -> ../a
	/../a -> /a
* Merge directory separator characters:
	a//b -> a/b
Return N of bytes written;
	<0 on error */
static inline ffssize ffpath_normalize(char *dst, ffsize cap, const char *src, ffsize len, ffuint flags)
{
	ffsize k = 0;
	ffstr s, part;
	ffstr_set(&s, src, len);
	int simplify = !!(flags & FFPATH_SIMPLE);

	const char *slashes = (flags & FFPATH_SLASH_BACKSLASH) ? "/\\" : "/";
#ifdef FF_WIN
	if ((flags & (FFPATH_SLASH_BACKSLASH | FFPATH_SLASH_ONLY)) == 0)
		slashes = "/\\";

	if ((flags & (FFPATH_DISK_LETTER | FFPATH_NO_DISK_LETTER)) == 0)
		flags |= FFPATH_DISK_LETTER;
#endif
	int skip_disk = (flags & (FFPATH_DISK_LETTER | FFPATH_SIMPLE)) == (FFPATH_DISK_LETTER | FFPATH_SIMPLE);

	if ((flags & FFPATH_DISK_LETTER)
		&& s.len > 2
		&& *(ffushort*)s.ptr == *(ffushort*)"\\\\") { // "\\.\", "\\?\", "\\host\"
		goto copy;
	}

	while (s.len != 0) {
		const char *s2 = s.ptr;
		ffssize pos = ffstr_splitbyany(&s, slashes, &part, &s);

		if (simplify) {
			if (part.len == 0
				|| ffstr_eqcz(&part, "."))
				continue; // merge slashes, skip dots

			if (skip_disk) {
				skip_disk = 0;
				if (*ffstr_last(&part) == ':')
					continue; // skip 'c:/'
			}

		} else {
			// allow leading slash or dot
			simplify = 1;
		}

		if (ffstr_eqcz(&part, "..")) {
			if (k != 0) {
				ffssize slash = ffs_rfindany(dst, k - 1, slashes, ffsz_len(slashes));
				ffstr prev;
				if (slash < 0) {
					ffstr_set(&prev, dst, k - 1);
					if (ffstr_eqcz(&prev, ".")) {
						k = 0; // "./" -> "../"
					} else if (ffstr_eqcz(&prev, "..")) {
						// "../" -> "../../"
					} else if (k == 1) {
						continue; // "/" -> "/"
					} else if ((flags & FFPATH_DISK_LETTER)
						&& *ffstr_last(&prev) == ':') {
						continue; // "c:/" -> "c:/"
					} else {
						k = 0; // "a/" -> ""
						continue;
					}

				} else {
					slash++;
					ffstr_set(&prev, &dst[slash], k - 1 - slash);
					if (ffstr_eqcz(&prev, "..")) {
						// ".../../" -> ".../../../"
					} else {
						k = slash; // ".../a/" -> ".../"
						continue;
					}
				}

			} else if (flags & FFPATH_SIMPLE) {
				continue;
			}
		}

		if (k + part.len + ((pos >= 0) ? 1 : 0) > cap)
			return -1;
		ffmem_move(&dst[k], part.ptr, part.len);
		k += part.len;

		if (pos >= 0) {
			if (flags & FFPATH_FORCE_SLASH)
				dst[k] = '/';
			else if (flags & FFPATH_FORCE_BACKSLASH)
				dst[k] = '\\';
			else
				dst[k] = s2[pos];
			k++;
		}
	}

	return k;

copy:
	if (s.len > cap)
		return -1;
	if (dst != s.ptr)
		ffmem_move(dst, s.ptr, s.len);
	return s.len;
}

enum FFPATH_NORM {
	/** Handle "/" path separator.
	Default on UNIX. */
	FFPATH_NORM_UNIX = 1,

	/** Handle "\" and "/" path separators.
	Convert "/" to "\".
	Normalize DOS paths, e.g. "c:" to "C:\".
	Pass through device paths and UNC paths.
	Default on Windows.

	Windows-specific path examples:
	* "c:\dir\file.txt" (DOS path)
	* "\\.\c:\dir\file.txt" (device path)
	* "\\?\c:\dir\file.txt" (device path)
	* "\\.\Volume{00000000-0000-0000-0000-000000000000}\dir\file.txt" (device path)
	* "\\localhost\c$\dir\file.txt" (UNC path)
	*/
	FFPATH_NORM_WINDOWS = 2,

	/** Strict bounds checking for "..". */
	FFPATH_NORM_STRICT = 0x10,
};

/** Normalize file path.
flags: enum FFPATH_NORM

* Merge directory separator characters:
	a//b -> a/b
* Skip "." components unless leading:
	a/./b -> a/b
	./a -> ./a
* Handle ".." components:
	a/../b -> b
	/../a -> /a
	../a -> ../a
* Remove trailing separator:
	/dir/ -> /dir

Return N of bytes written;
 <0 on error */
static inline int ffpath_norm(char *dst, ffsize cap, const char *src, ffsize len, ffuint flags)
{
	if (!(flags & (FFPATH_NORM_UNIX | FFPATH_NORM_WINDOWS))) {
#ifdef FF_WIN
		flags |= FFPATH_NORM_WINDOWS;
#else
		flags |= FFPATH_NORM_UNIX;
#endif
	}

	int r;
	ffushort parts[255];
	ffuint n = 0, k = 0, root, cur = 0;
	int slash = (flags & FFPATH_NORM_WINDOWS) ? '\\' : '/';
	const char *p = src;

	// Root:

	if (flags & FFPATH_NORM_WINDOWS) {

		if (len > 2
			&& src[0] == '\\' && src[1] == '\\') {

			// "\\.\", "\\?\", "\\host\"
			if (len > cap)
				return -1;
			if (dst != src)
				ffmem_move(dst, src, len);
			return len;
		}

		if (len >= 2
			&& ((src[0] & ~0x20) >= 'A' && (src[0] & ~0x20) <= 'Z'
				&& src[1] == ':')) {

			// DOS path: "c:" -> "C:\"
			if (k + 3 > cap)
				return -1;
			dst[k++] = src[0] & ~0x20; // 'a' -> 'A'
			dst[k++] = ':';
			dst[k++] = '\\';
			p += 2 + !!(len >= 3 && (src[2] == '/' || src[2] == '\\'));

		} else if (len && (src[0] == '/' || src[0] == '\\')) {
			// Root: "/" or "\" -> "\"
			dst[k++] = '\\';
			p++;
		}

	} else if (len && src[0] == '/') {
		// UNIX root
		dst[k++] = '/';
		p++;
	}
	parts[n++] = p - src;
	root = k;

	// Components:

	if (len > 4095)
		return -1; // Too large input

	for (;;) {
		if (flags & FFPATH_NORM_WINDOWS)
			r = ffs_findany(p, src + len - p, "/\\", 2);
		else
			r = ffs_findchar(p, src + len - p, '/');
		if (r < 0)
			r = src + len - p;

		ffuint f = 0;
		if (r == 1 && p[0] == '.')
			f = 0x1000;
		else if (r == 2 && p[0] == '.' && p[1] == '.')
			f = 0x2000;

		if (n == FF_COUNT(parts))
			return -1; // Too large input
		parts[n++] = f | (p - src + r);

		p += r + 1;
		if (p > src + len) {
			break;
		}
	}

	// Output path:

	for (ffuint i = 1;  i < n;  i++, cur++) {
		ffuint off = (parts[i - 1] & 0x0fff);
		ffuint plen = (parts[i] & 0x0fff) - (parts[i - 1] & 0x0fff);
		if (i != 1) {
			// Skip slash
			plen--;
			off++;
		}

		if (plen == 0)
			continue; // "//" -> "/"

		if (parts[i] & 0x1000) {
			if (k)
				continue; // dst="/" + "." -> "/"
			// dst="" + "." -> "."

		} else if (parts[i] & 0x2000) {
			if (k == root) {
				if (flags & FFPATH_NORM_STRICT)
					return -1;
				if (root)
					continue; // dst="/" + ".." -> "/"
				// dst="" + ".." -> ".."

			} else {
				ffuint parent_len = (parts[cur] & 0x0fff) - (parts[cur - 1] & 0x0fff);
				if (parts[cur] & 0x1000) {
					k -= parent_len;
					// dst="." + ".." -> ".."

				} else if (parts[cur] & 0x2000) {
					// dst=".." + ".." -> "../.."

				} else {
					k -= parent_len;
					cur -= 2;
					continue; // dst="a" + ".." -> ""
				}
			}
		}

		if (k + (k != root) + plen > cap)
			return -1; // Output buffer too small
		if (k != root)
			dst[k++] = slash;
		ffmem_move(dst + k, src + off, plen);
		k += plen;
	}

	return k;
}
