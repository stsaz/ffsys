/**
2013 Simon Zolin */

#include <ffsys/string.h>
#include <ffsys/dir.h>
#include <ffsys/dirscan.h>
#include <ffsys/path.h>
#include <ffsys/file.h>
#include <ffsys/error.h>
#include <ffsys/test.h>
#include <ffbase/stringz.h>


#ifdef FF_UNIX
#define TMP_PATH "/tmp"
#else
#define TMP_PATH "."
#endif

#define FFSTR(s)  (char*)(s), FFS_LEN(s)

static int test_pathinfo(const char *dirname)
{
	(void)dirname;

	x(ffpath_abs(FFSTR("/absolute/path")));
	x(!ffpath_abs(FFSTR("./relative/path")));
#ifdef FF_WIN
	x(ffpath_abs(FFSTR("\\absolute\\path")));
	x(ffpath_abs(FFSTR("c:\\absolute\\path")));
#endif

#ifdef FF_WIN
	{
		wchar_t wname[256];
		if (0 >= ffsz_utow(wname, FF_COUNT(wname), dirname))
			return -1;
		x(ffpath_islong(wname));
	}
#endif

	ffpathinfo st = {};
	x_sys(0 == ffpath_infoinit(".", &st));
	x_sys(0 != ffpath_info(&st, FFPATH_BLOCKSIZE));

	return 0;
}

static int test_dirwalk(char *path, ffsize pathcap)
{
	ffdir d;
	ffdirentry ent;
	int nfiles = 0;

	d = ffdir_open(path, pathcap, &ent);
	x(d != NULL);

	for (;;) {
		const char *name;
		const fffileinfo *fi;

		if (0 != ffdir_read(d, &ent)) {
			x(fferr_last() == FFERR_NOMOREFILES);
			break;
		}

		name = ffdir_entry_name(&ent);
		x(name != NULL);

		fi = ffdir_entry_info(&ent);
		x(fi != NULL);

		if (!ffsz_cmp(name, ".")) {
			x(fffile_isdir(fffileinfo_attr(fi)));
			nfiles++;
		}
		else if (!ffsz_cmp(name, "tmpfile")) {
			x(!fffile_isdir(fffileinfo_attr(fi)));
			x(1 == fffileinfo_size(fi));
			nfiles++;
		}
	}
	x(nfiles == 2);

	ffdir_close(d);
	return 0;
}

void test_chdir(const char *dir)
{
#ifdef FF_WIN
	SetCurrentDirectoryA(dir);
#else
	chdir(dir);
#endif
}

void test_dirscan()
{
	x(_ffdirscan_filename_cmpz("/a", "/a") == 0);
	x(_ffdirscan_filename_cmpz("/A", "/A") == 0);

	x(_ffdirscan_filename_cmpz("/a", "/b") < 0);
	x(_ffdirscan_filename_cmpz("/b", "/a") > 0);

	x(_ffdirscan_filename_cmpz("/a", "/A") < 0);
	x(_ffdirscan_filename_cmpz("/A", "/a") > 0);

	x(_ffdirscan_filename_cmpz("/ab", "/Aa") > 0);

	x(_ffdirscan_filename_cmpz("/a", "/B") < 0);
	x(_ffdirscan_filename_cmpz("/B", "/a") > 0);

	const char *name;
	static const char *names[] = {
		"ffsystest-dirscan",
			"filea",
			"fileb",
			"filec",
			"zdir",
	};
	ffdir_make(names[0]);
	test_chdir(names[0]);
	int i;
	for (i = 1;  i != FF_COUNT(names) - 1;  i++) {
		fffile_writewhole(names[i], "123", 3, 0);
	}
	ffdir_make(names[i]);

// default
	ffdirscan d = {};
	x_sys(0 == ffdirscan_open(&d, ".", 0));
	for (i = 1;  ;  i++) {
		if (NULL == (name = ffdirscan_next(&d))) {
			xieq(i, FF_COUNT(names));
			break;
		}
		xsz(name, names[i]);
	}
	ffdirscan_close(&d);

// wildcard
	d.wildcard = "f*b";
	x_sys(0 == ffdirscan_open(&d, ".", FFDIRSCAN_USEWILDCARD));
	for (;;) {
		if (NULL == (name = ffdirscan_next(&d)))
			break;
		xsz(name, names[2]);
	}
	ffdirscan_close(&d);

// dont skip dot
	x_sys(0 == ffdirscan_open(&d, ".", FFDIRSCAN_DOT));
	for (i = 1;  ;  i++) {
		if (NULL == (name = ffdirscan_next(&d)))
			break;
		if (i == 1)
			xsz(name, ".");
		else if (i == 2)
			xsz(name, "..");
		else
			break;
	}
	ffdirscan_close(&d);

#ifdef FF_LINUX
// fd
	fffd f = fffile_open(".", FFFILE_READONLY);
	x(f != FFFILE_NULL);
	d.fd = f;
	x_sys(0 == ffdirscan_open(&d, ".", FFDIRSCAN_USEFD));
	for (i = 1;  ;  i++) {
		if (NULL == (name = ffdirscan_next(&d)))
			break;
		xsz(name, names[i]);
	}
	ffdirscan_close(&d);
#endif

// dir-first
	ffdirscanx dx = {};
	x_sys(!ffdirscanx_open(&dx, ".", FFDIRSCANX_SORT_DIRS));
	x(!!(name = ffdirscanx_next(&dx)));
	xsz(name, names[FF_COUNT(names) - 1]);
	for (i = 1;  ;  i++) {
		if (!(name = ffdirscanx_next(&dx))) {
			xieq(i, FF_COUNT(names));
			break;
		}
		xsz(name, names[i]);
	}
	ffdirscanx_close(&dx);

	for (i = 1;  i != FF_COUNT(names) - 1;  i++) {
		fffile_remove(names[i]);
	}
	ffdir_remove(names[i]);

	test_chdir("..");
	ffdir_remove(names[0]);
}

void test_dir()
{
	fffd f;
	char path[256];
	char fn[256];
	ffsize pathcap = FF_COUNT(path);

	strcpy(path, TMP_PATH);
	strcat(path, "/tmpdir");

	strcpy(fn, path);
	strcat(fn, "/tmpfile");

	ffdir_make(path);

	f = fffile_open(fn, FFFILE_CREATE | FFFILE_TRUNCATE | FFFILE_WRITEONLY);
	x(f != FFFILE_NULL);
	x(1 == fffile_write(f, FFSTR("1")));
	x(0 == fffile_close(f));

	test_dirwalk(path, pathcap);
	test_pathinfo(path);

	x(0 == fffile_remove(fn));
	x(0 == ffdir_remove(path));


	strcpy(path, TMP_PATH);
	strcat(path, "/tmpdir/tmpdir2");
	x(0 == ffdir_make_all(path, strlen(TMP_PATH)));
	x(0 == ffdir_remove(path));
	path[strlen(path) - FFS_LEN("/tmpdir2")] = '\0';
	x(0 == ffdir_remove(path));
}
