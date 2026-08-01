/** ffsys: std.h tester
2020, Simon Zolin */

#include <ffsys/std.h>
#include <ffsys/file.h>
#include <ffsys/queue.h>
#include <ffsys/test.h>

#ifdef FF_WIN
#define S1  "Привет\r\n"
#else
#define S1  "Привет\n"
#endif
#define STR2(s)  (s), FFS_LEN(s)

void test_std()
{
	ffstr s;
	ffuint n;
	x(FFKEY_ENTER == ffstd_key_parse_unix(STR2("\x0d"), &n) && n == 1);
	x(0 == ffstd_key_parse_unix(STR2("\x1b"), &n));
	x('A' == ffstd_key_parse_unix(STR2("A"), &n) && n == 1);
	x(*(int*)"\xd0\xaf\0\0" == ffstd_key_parse_unix(STR2("Я"), &n) && n == 2);
	x(*(int*)"\xF0\x9F\x98\x80" == ffstd_key_parse_unix(STR2("\xF0\x9F\x98\x80"), &n) && n == 4);
	x((FFKEY_ALT | *(int*)"\xd0\xaf\0\0") == ffstd_key_parse_unix(STR2("\eЯ"), &n) && n == 3);
	x((FFKEY_ALT | 'A') == ffstd_key_parse_unix(STR2("\eA"), &n) && n == 2);
	x(FFKEY_UP == ffstd_key_parse_unix(STR2("\e[\x41"), &n) && n == 3);
	x((FFKEY_CTRL | FFKEY_UP) == ffstd_key_parse_unix(STR2("\e[1;5\x41"), &n) && n == 6);
	x(FFKEY_INS == ffstd_key_parse_unix(STR2("\e[2~"), &n) && n == 4);
	x((FFKEY_CTRL | FFKEY_INS) == ffstd_key_parse_unix(STR2("\e[2;5~"), &n) && n == 6);
	x(FFKEY_F1 == ffstd_key_parse_unix(STR2("\e\x4f\x50"), &n) && n == 3);
	x((FFKEY_CTRL | FFKEY_F1) == ffstd_key_parse_unix(STR2("\e\x4f\x35\x50"), &n) && n == 4);
	x(FFKEY_F5 == ffstd_key_parse_unix(STR2("\e[15~"), &n) && n == 5);
	x((FFKEY_CTRL | FFKEY_F5) == ffstd_key_parse_unix(STR2("\e[15;5~"), &n) && n == 7);
	x(FFKEY_F9 == ffstd_key_parse_unix(STR2("\e[20~"), &n) && n == 5);
	x((FFKEY_CTRL | FFKEY_F9) == ffstd_key_parse_unix(STR2("\e[20;5~"), &n) && n == 7);
	x(FFKEY_TEXT_PASTED == ffstd_key_parse_unix(STR2("\e[200~"), &n) && n == 6);
	x(12+4 == ffstd_paste_read(STR2("\e[200~text\e[201~"), &s) && ffstr_eqz(&s, "text"));
	x(-1 == ffstd_key_parse_unix(STR2("\e[X"), &n));

	xint_sys(FFS_LEN(S1), ffstdout_write(S1, FFS_LEN(S1)));
	xint_sys(FFS_LEN(S1), ffstderr_write(S1, FFS_LEN(S1)));

#if 0
	fflog("please enter 'Привет' and press <Enter>");
	char buf[100];
	xint_sys(FFS_LEN(S1), ffstdin_read(buf, sizeof(buf)));
	x(!ffmem_cmp(buf, S1, FFS_LEN(S1)));
#endif
}

void test_std_event()
{
	ffstd_title("ffsys", 5);

	ffuint attr = FFSTD_LINEINPUT | FFSTD_ECHO;
	x_sys(0 == ffstd_attr(ffstdin, attr, 0));

	ffkq kq = ffkq_create();
	x_sys(kq != FFKQ_NULL);
#ifdef FF_UNIX
	fffile_nonblock(ffstdin, 1);
	x_sys(0 == ffkq_attach(kq, ffstdin, NULL, FFKQ_READ));
#endif

	fflog("Press Ctrl+Right");
	char buf[512];
	int r;
	ffuint key, n;
	ffstr data = {};

	for (;;) {
		r = ffstd_key_read(ffstdin, buf, sizeof(buf));
		fflog("ffstd_key_read: %d  %*xb"
			, r, (r >= 0) ? (ffsize)r : (ffsize)0, buf);
		ffstr_set(&data, buf, r);

		if (r == 0) {
			fflog("waiting for key input...");

#ifdef FF_WIN
			r = WaitForSingleObject(ffstdin, 10000);
			xint_sys(WAIT_OBJECT_0, r);
			ffkq_postevent post = ffkq_post_attach(kq, NULL);
			x_sys(0 == ffkq_post(post, NULL));
#endif

			ffkq_event ev;
			ffkq_time t;
			ffkq_time_set(&t, 10000);
			r = ffkq_wait(kq, &ev, 1, t);
			xint_sys(1, r);
			continue;
		}
		x_sys(r > 0);

		key = ffstd_key_parse(data.ptr, data.len, &n);
		if (key == (ffuint)(FFKEY_CTRL | FFKEY_RIGHT))
			break;
		ffstr_shift(&data, n);
	}
	xint_sys((ffuint)(FFKEY_CTRL | FFKEY_RIGHT), key);

	x_sys(0 == ffstd_attr(ffstdin, attr, attr));
	ffkq_close(kq);
}
