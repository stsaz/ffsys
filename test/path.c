/** ffsys: path.h tester
2020, Simon Zolin */

#include <ffsys/path.h>
#include <ffsys/test.h>

#define STR2(s)  (char*)(s), FFS_LEN(s)

void test_path_split()
{
	ffstr nm, ext;
	ffpath_splitname(STR2("file.txt"), &nm, &ext);
	xseq(&nm, "file");
	xseq(&ext, "txt");

	ffpath_splitname(STR2("qwer"), &nm, &ext);
	xseq(&nm, "qwer");
	xseq(&ext, "");

	ffpath_splitname(STR2(".qwer"), &nm, &ext);
	xseq(&nm, ".qwer");
	xseq(&ext, "");

	ffstr dir;
	x(FFS_LEN("/path/to") == ffpath_splitpath(STR2("/path/to/file"), &dir, &nm));
	xseq(&dir, "/path/to");
	xseq(&nm, "file");

	x(-1 == ffpath_splitpath(STR2("file"), &dir, &nm));
	xseq(&dir, "");
	xseq(&nm, "file");
}

void test_path_isroot()
{
	x(ffpath_isroot("/", 1));
	x(!ffpath_isroot("/a", 2));
	x(!ffpath_isroot("a", 1));
	x(!ffpath_isroot("", 0));

#ifdef FF_WIN
	x(ffpath_isroot("\\", 1));
	x(ffpath_isroot("/", 1));

	x(ffpath_isroot("c:", 2));
	x(ffpath_isroot("C:", 2));
	x(!ffpath_isroot("c", 1));

	x(ffpath_isroot("c:\\", 3));
	x(ffpath_isroot("c:/", 3));
	x(ffpath_isroot("C:\\", 3));

	x(!ffpath_isroot("c:\\a", 5));
	x(!ffpath_isroot("c:ab", 4));
#endif
}

void test_path_normalize()
{
	char buf[32];
	ffstr s;
	s.ptr = buf;

	// normal
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("a/b"), 0);
	xseq(&s, "a/b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("/a/b/"), 0);
	xseq(&s, "/a/b/");

	// merge slashes
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("a//b"), 0);
	xseq(&s, "a/b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("a//b//"), 0);
	xseq(&s, "a/b/");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("//a\\\\b//c"), FFPATH_SLASH_BACKSLASH | FFPATH_FORCE_SLASH);
	xseq(&s, "/a/b/c");

	// .
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("./a/b"), 0);
	xseq(&s, "./a/b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("./a/./b"), 0);
	xseq(&s, "./a/b");

	// ..
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("../a/b"), 0);
	xseq(&s, "../a/b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("../../a"), 0);
	xseq(&s, "../../a");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("a/../b"), 0);
	xseq(&s, "b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("./a/../../b"), 0);
	xseq(&s, "../b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("../a/../../b"), 0);
	xseq(&s, "../../b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("/a/../../b"), 0);
	xseq(&s, "/b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("c:/../../b"), 0);
#ifdef FF_WIN
	xseq(&s, "c:/b");
#else
	xseq(&s, "../b");
#endif
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("c:/../../b"), FFPATH_DISK_LETTER);
	xseq(&s, "c:/b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("c:/../../b"), FFPATH_NO_DISK_LETTER);
	xseq(&s, "../b");

	// slash
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("a\\b/c"), FFPATH_FORCE_SLASH);
#ifdef FF_WIN
	xseq(&s, "a/b/c");
#else
	xseq(&s, "a\\b/c");
#endif
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("a\\b/c"), FFPATH_SLASH_ONLY | FFPATH_FORCE_SLASH);
	xseq(&s, "a\\b/c");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("a\\b/c"), FFPATH_SLASH_BACKSLASH | FFPATH_FORCE_SLASH);
	xseq(&s, "a/b/c");

	// simple
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("/"), FFPATH_SIMPLE);
	xseq(&s, "");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("./a/./b"), FFPATH_SIMPLE);
	xseq(&s, "a/b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("./a/../../b"), FFPATH_SIMPLE);
	xseq(&s, "b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("../a/../b"), FFPATH_SIMPLE);
	xseq(&s, "b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("../a/../../b"), FFPATH_SIMPLE);
	xseq(&s, "b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("/a/../../b"), FFPATH_SIMPLE);
	xseq(&s, "b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("/a/b"), FFPATH_SIMPLE);
	xseq(&s, "a/b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("c:/a/b"), FFPATH_SIMPLE);
#ifdef FF_WIN
	xseq(&s, "a/b");
#else
	xseq(&s, "c:/a/b");
#endif
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("c:/a/b"), FFPATH_SIMPLE | FFPATH_DISK_LETTER);
	xseq(&s, "a/b");

	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("\\\\.\\c:\\dir\\file"), FFPATH_DISK_LETTER);
	xseq(&s, "\\\\.\\c:\\dir\\file");

	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("\\\\srv\\dir\\file"), FFPATH_DISK_LETTER);
	xseq(&s, "\\\\srv\\dir\\file");
}

void test_path_norm()
{
	char buf[100];

	// Absolute:
	xstrn("/", buf, ffpath_norm(buf, sizeof(buf), STR2("/"), FFPATH_NORM_UNIX));
	xstrn("/", buf, ffpath_norm(buf, sizeof(buf), STR2("//"), FFPATH_NORM_UNIX));
	xstrn("/", buf, ffpath_norm(buf, sizeof(buf), STR2("/."), FFPATH_NORM_UNIX));
	xstrn("/", buf, ffpath_norm(buf, sizeof(buf), STR2("/.."), FFPATH_NORM_UNIX));
	xstrn("/a/b", buf, ffpath_norm(buf, sizeof(buf), STR2("/a/b/"), FFPATH_NORM_UNIX));
	xstrn("/a/b", buf, ffpath_norm(buf, sizeof(buf), STR2("/a/./b"), FFPATH_NORM_UNIX));
	xstrn("/b", buf, ffpath_norm(buf, sizeof(buf), STR2("/a/../b"), FFPATH_NORM_UNIX));
	xstrn("/a/c", buf, ffpath_norm(buf, sizeof(buf), STR2("/a/b/../c"), FFPATH_NORM_UNIX));
	xstrn("/b", buf, ffpath_norm(buf, sizeof(buf), STR2("/a/../../b"), FFPATH_NORM_UNIX));

	// Relative:
	xstrn(".", buf, ffpath_norm(buf, sizeof(buf), STR2("."), FFPATH_NORM_UNIX));
	xstrn("..", buf, ffpath_norm(buf, sizeof(buf), STR2(".."), FFPATH_NORM_UNIX));
	xstrn("a", buf, ffpath_norm(buf, sizeof(buf), STR2("a"), FFPATH_NORM_UNIX));
	xstrn("a/b", buf, ffpath_norm(buf, sizeof(buf), STR2("a/b"), FFPATH_NORM_UNIX));
	xstrn("a/b", buf, ffpath_norm(buf, sizeof(buf), STR2("a/./b"), FFPATH_NORM_UNIX));
	xstrn("../b", buf, ffpath_norm(buf, sizeof(buf), STR2("a/../../b"), FFPATH_NORM_UNIX));
	xstrn("./a", buf, ffpath_norm(buf, sizeof(buf), STR2("././a"), FFPATH_NORM_UNIX));
	xstrn("../../a", buf, ffpath_norm(buf, sizeof(buf), STR2("./../../a"), FFPATH_NORM_UNIX));
	xstrn("../../a", buf, ffpath_norm(buf, sizeof(buf), STR2("../../a"), FFPATH_NORM_UNIX));
	xstrn("c", buf, ffpath_norm(buf, sizeof(buf), STR2("a/b/../../c"), FFPATH_NORM_UNIX));

	// FFPATH_NORM_WINDOWS:
	xstrn("C:\\", buf, ffpath_norm(buf, sizeof(buf), STR2("c:"), FFPATH_NORM_WINDOWS));
	xstrn("C:\\", buf, ffpath_norm(buf, sizeof(buf), STR2("c:\\"), FFPATH_NORM_WINDOWS));
	xstrn("C:\\b", buf, ffpath_norm(buf, sizeof(buf), STR2("c:/a/../../b"), FFPATH_NORM_WINDOWS));
	xstrn("C:\\a\\b", buf, ffpath_norm(buf, sizeof(buf), STR2("c:/a/b/"), FFPATH_NORM_WINDOWS));
	xstrn("\\a\\b", buf, ffpath_norm(buf, sizeof(buf), STR2("/a/b/"), FFPATH_NORM_WINDOWS));
	xstrn("\\\\srv\\dir\\file", buf, ffpath_norm(buf, sizeof(buf), STR2("\\\\srv\\dir\\file"), FFPATH_NORM_WINDOWS));

	// FFPATH_NORM_STRICT:
	x(-1 == ffpath_norm(buf, sizeof(buf), STR2("/../a"), FFPATH_NORM_UNIX | FFPATH_NORM_STRICT));
	x(-1 == ffpath_norm(buf, sizeof(buf), STR2("a/../../b"), FFPATH_NORM_UNIX | FFPATH_NORM_STRICT));
	xstrn("d", buf, ffpath_norm(buf, sizeof(buf), STR2("a/b/c/../../../d"), FFPATH_NORM_UNIX | FFPATH_NORM_STRICT));

	// Edge-cases:
	x(0 == ffpath_norm(buf, sizeof(buf), STR2(""), FFPATH_NORM_UNIX));
	xstrn("/a", buf, ffpath_norm(buf, 2, STR2("/a"), FFPATH_NORM_UNIX));
	x(-1 == ffpath_norm(buf, 1, STR2("/a"), FFPATH_NORM_UNIX));
}

void test_path()
{
	test_path_isroot();
	test_path_split();
	test_path_normalize();
	test_path_norm();
}
