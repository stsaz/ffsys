/** ffsys: standard I/O
2020, Simon Zolin */

/*
ffstdin_read
ffstdout_write ffstderr_write
ffstdout_fmt ffstderr_fmt
fflog fflogz
ffstd_keyread
ffstd_keyparse
ffstd_attr
*/

#pragma once
#include <ffsys/string.h>

enum FFKEY {
	FFKEY_BACKSPACE = 0x08,
	FFKEY_TAB = 0x09,
	FFKEY_ENTER = 0x0d,
	FFKEY_ESCAPE = 0x1b,

	FFKEY_VIRT = 1 << 31,
	FFKEY_DEL,
	FFKEY_INS,
	FFKEY_UP,
	FFKEY_DOWN,
	FFKEY_RIGHT,
	FFKEY_LEFT,
	FFKEY_HOME,
	FFKEY_END,
	FFKEY_PGUP,
	FFKEY_PGDN,
	FFKEY_F1,
	FFKEY_F2,
	FFKEY_F3,
	FFKEY_F4,
	FFKEY_F5,
	FFKEY_F6,
	FFKEY_F7,
	FFKEY_F8,
	FFKEY_F9,
	FFKEY_F10,
	FFKEY_F11,
	FFKEY_F12,

	FFKEY_CTRL = 1 << 24,
	FFKEY_SHIFT = 2 << 24,
	FFKEY_ALT = 4 << 24,
	FFKEY_MODMASK = FFKEY_CTRL | FFKEY_SHIFT | FFKEY_ALT,
};

enum FFSTD_ATTR {
	FFSTD_ECHO = 1, // set echo on/off (stdin)
	FFSTD_LINEINPUT = 2, // set line-input/character-input (stdin)
	FFSTD_VTERM = 4, // Windows: enable control character sequences
};


#define FFSTD_BLACK  "0"
#define FFSTD_RED  "1"
#define FFSTD_GREEN  "2"
#define FFSTD_YELLOW  "3"
#define FFSTD_BLUE  "4"
#define FFSTD_PURPLE  "5"
#define FFSTD_CYAN  "6"
#define FFSTD_WHITE  "7"

#define FFSTD_CLR_RESET  "\033[0m"

/** Normal */
#define FFSTD_CLR(f)      "\033["   "3" f "m"
/** Intense */
#define FFSTD_CLR_I(f)    "\033["   "9" f "m"
/** Bold/bright */
#define FFSTD_CLR_B(f)    "\033[1;" "3" f "m"

/** Background normal */
#define FFSTD_CLRBG(b)    "\033["   "4" b "m"
/** Background intense */
#define FFSTD_CLRBG_I(b)  "\033["  "10" b "m"


#ifdef FF_WIN

#include <ffbase/slice.h>

#define ffstdin  GetStdHandle(STD_INPUT_HANDLE)
#define ffstdout  GetStdHandle(STD_OUTPUT_HANDLE)
#define ffstderr  GetStdHandle(STD_ERROR_HANDLE)

static inline ffssize ffstdin_read(void *buf, ffsize cap)
{
	DWORD read;
	HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
	if (GetConsoleMode(h, &read)) {

		wchar_t ws[1024], *w = ws;
		FF_ASSERT(cap >= 4);
		ffsize wcap = cap / 4;
		if (cap / 4 > FF_COUNT(ws)) {
			if (NULL == (w = (wchar_t*)ffmem_alloc(cap / 2)))
				return -1;
		}

		BOOL b = ReadConsoleW(h, w, ffmin(wcap, 0xffffffff), &read, NULL);

		ffssize r;
		if (b) {
			r = ffs_wtou((char*)buf, cap, w, read);
			if (r < 0)
				b = 0;
		}

		if (w != ws)
			ffmem_free(w);
		return (b) ? r : -1;
	}

	if (!ReadFile(h, buf, ffmin(cap, 0xffffffff), &read, 0))
		return -1;
	return read;
}

static inline ffssize _ffstd_write(HANDLE h, const void *data, ffsize len)
{
	DWORD written;
	if (GetConsoleMode(h, &written)) {
		wchar_t ws[1024], *w;
		ffsize r = FF_COUNT(ws);
		w = ffs_alloc_buf_utow(ws, &r, (char*)data, len);

		BOOL b = WriteConsoleW(h, w, r, &written, NULL);

		if (w != ws)
			ffmem_free(w);
		return (b) ? (ffssize)len : -1;
	}

	if (!WriteFile(h, data, len, &written, 0))
		return -1;
	return written;
}

static inline ffssize ffstdout_write(const void *data, ffsize len)
{
	return _ffstd_write(GetStdHandle(STD_OUTPUT_HANDLE), data, len);
}

static inline ffssize ffstderr_write(const void *data, ffsize len)
{
	return _ffstd_write(GetStdHandle(STD_ERROR_HANDLE), data, len);
}

typedef struct ffstd_ev {
	ffuint state;
	INPUT_RECORD rec[8];
	ffuint irec, nrec;
} ffstd_ev;

static inline int ffstd_keyread(fffd fd, ffstd_ev *ev, ffstr *data)
{
	for (;;) {
		switch (ev->state) {

		case 0: {
			DWORD n;
			if (!GetNumberOfConsoleInputEvents(fd, &n))
				return -1;
			if (n == 0)
				return 0;

			if (!ReadConsoleInput(fd, ev->rec, ffmin(n, FF_COUNT(ev->rec)), &n))
				return -1;
			if (n == 0)
				return 0;
			ev->nrec = n;
			ev->irec = 0;
			ev->state = 1;
		}
		// fallthrough

		case 1:
			if (ev->irec == ev->nrec) {
				ev->state = 0;
				continue;
			}

			if (!(ev->rec[ev->irec].EventType == KEY_EVENT && ev->rec[ev->irec].Event.KeyEvent.bKeyDown)) {
				ev->irec++;
				continue;
			}

			ffstr_set(data, (void*)&ev->rec[ev->irec].Event.KeyEvent, sizeof(ev->rec[ev->irec].Event.KeyEvent));
			ev->irec++;
			return data->len;
		}
	}
}

static inline int ffstd_keyparse(ffstr *data)
{
	if (data->len < sizeof(KEY_EVENT_RECORD))
		return -1;

	const KEY_EVENT_RECORD *k = (KEY_EVENT_RECORD*)data->ptr;
	ffuint r = k->uChar.AsciiChar;
	if (r == 0) {
		if (k->wVirtualKeyCode >= VK_PRIOR && k->wVirtualKeyCode <= VK_DELETE) {
			static const ffbyte keys_vk[] = {
				[VK_PRIOR - VK_PRIOR]   = FFKEY_PGUP & 0xff,
				[VK_NEXT - VK_PRIOR]    = FFKEY_PGDN & 0xff,
				[VK_END - VK_PRIOR]     = FFKEY_END & 0xff,
				[VK_HOME - VK_PRIOR]    = FFKEY_HOME & 0xff,
				[VK_LEFT - VK_PRIOR]    = FFKEY_LEFT & 0xff,
				[VK_UP - VK_PRIOR]      = FFKEY_UP & 0xff,
				[VK_RIGHT - VK_PRIOR]   = FFKEY_RIGHT & 0xff,
				[VK_DOWN - VK_PRIOR]    = FFKEY_DOWN & 0xff,
				[VK_INSERT - VK_PRIOR]  = FFKEY_INS & 0xff,
				[VK_DELETE - VK_PRIOR]  = FFKEY_DEL & 0xff,
			};
			r = FFKEY_VIRT | keys_vk[k->wVirtualKeyCode - VK_PRIOR];

		} else if (k->wVirtualKeyCode >= VK_F1 && k->wVirtualKeyCode <= VK_F12) {
			r = (ffuint)(k->wVirtualKeyCode - VK_F1 + FFKEY_F1);

		} else {
			return -1;
		}
	}

	ffuint ctl = k->dwControlKeyState;
	if (ctl & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED))
		r |= FFKEY_ALT;
	if (ctl & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED))
		r |= FFKEY_CTRL;
	if (ctl & SHIFT_PRESSED)
		r |= FFKEY_SHIFT;

	data->len = 0;
	return r;
}

static inline int ffstd_attr(fffd fd, ffuint attr, ffuint val)
{
	DWORD mode;
	if (!GetConsoleMode(fd, &mode))
		return -1;

	if ((attr & FFSTD_VTERM) && (val & FFSTD_VTERM)) {
		mode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	}

	if (attr & FFSTD_ECHO) {
		if (val & FFSTD_ECHO)
			mode |= ENABLE_ECHO_INPUT;
		else
			mode &= ~ENABLE_ECHO_INPUT;
	}

	if (attr & FFSTD_LINEINPUT) {
		if (val & FFSTD_LINEINPUT)
			mode |= ENABLE_LINE_INPUT;
		else
			mode &= ~ENABLE_LINE_INPUT;
	}

	return !SetConsoleMode(fd, mode);
}

#else // UNIX:

#include <ffsys/error.h>
#include <sys/stat.h>
#include <termios.h>

#define ffstdin  0
#define ffstdout  1
#define ffstderr  2

static inline ffssize ffstdin_read(void *buf, ffsize cap)
{
	return read(0, buf, cap);
}

static inline ffssize ffstdout_write(const void *data, ffsize len)
{
	return write(1, data, len);
}

static inline ffssize ffstderr_write(const void *data, ffsize len)
{
	return write(2, data, len);
}

typedef struct ffstd_ev {
	char buf[8];
} ffstd_ev;

static inline int ffstd_keyread(fffd fd, ffstd_ev *ev, ffstr *data)
{
	ffssize r = read(fd, ev->buf, sizeof(ev->buf));
	if (r < 0 && fferr_again(errno))
		return 0;
	else if (r == 0) {
		errno = EINVAL;
		return -1;
	}
	ffstr_set(data, ev->buf, r);
	return r;
}

/* Algorithm for escape-sequences:
1b4f:
	32..38 -> Shift..SAC
		50..53 -> F1..F4
	50..53 -> F1..F4
1b5b:
	31:
		35..39 -> F5..F8
			3b:
				32..38 -> Shift..SAC
					7e
			7e
		3b:
			32..38 -> Shift..SAC
				41..48 -> UP..HOME
	32:
		30..34 -> F9..F12
	32..36 -> INS..PGDN
		3b:
			32..38 -> Shift..SAC
				7e
		7e
	41..48 -> UP..HOME
*/
static inline int ffstd_keyparse(ffstr *data)
{
	int r = 0, i = 0;
	if (data->len == 0)
		return -1;
	if (data->len == 1) {
		r = data->ptr[0];
		ffstr_shift(data, 1);
		return r;
	}
	ffbyte d[8] = {};
	ffmem_copy(d, data->ptr, ffmin(data->len, 8));

	static const ffbyte keys_mod[] = {
		0,
		0,
		(FFKEY_SHIFT) >> 24,
		(FFKEY_ALT) >> 24,
		(FFKEY_SHIFT | FFKEY_ALT) >> 24,
		(FFKEY_CTRL) >> 24,
		(FFKEY_CTRL | FFKEY_SHIFT) >> 24,
		(FFKEY_CTRL | FFKEY_ALT) >> 24,
		(FFKEY_CTRL | FFKEY_SHIFT | FFKEY_ALT) >> 24,
	};
	static const ffbyte keys_f5_f8[] = {
		[0x35 - 0x30] = FFKEY_F5 & 0xff,
		[0x37 - 0x30] = FFKEY_F6 & 0xff,
		[0x38 - 0x30] = FFKEY_F7 & 0xff,
		[0x39 - 0x30] = FFKEY_F8 & 0xff,
	};
	static const ffbyte keys_f9_f12[] = {
		[0x30 - 0x30] = FFKEY_F9 & 0xff,
		[0x31 - 0x30] = FFKEY_F10 & 0xff,
		[0x33 - 0x30] = FFKEY_F11 & 0xff,
		[0x34 - 0x30] = FFKEY_F12 & 0xff,
	};
	static const ffbyte keys_3x[] = {
		[0x32 - 0x30] = FFKEY_INS & 0xff,
		[0x33 - 0x30] = FFKEY_DEL & 0xff,
		[0x35 - 0x30] = FFKEY_PGUP & 0xff,
		[0x36 - 0x30] = FFKEY_PGDN & 0xff,
	};
	static const ffbyte keys_4x[] = {
		[0x41 - 0x40] = FFKEY_UP & 0xff,
		[0x42 - 0x40] = FFKEY_DOWN & 0xff,
		[0x43 - 0x40] = FFKEY_RIGHT & 0xff,
		[0x44 - 0x40] = FFKEY_LEFT & 0xff,
		[0x46 - 0x40] = FFKEY_END & 0xff,
		[0x48 - 0x40] = FFKEY_HOME & 0xff,
	};

	if (d[0] == 0x1b && d[1] == 0x4f) {
		i = 2;
		if (d[2] >= 0x32 && d[2] <= 0x38) {
			r = (ffuint)keys_mod[d[2] - 0x30] << 24;
			i = 3;
		}

		if (d[i] >= 0x50 && d[i] <= 0x53) {
			r |= FFKEY_F1 + d[i] - 0x50;
			goto fin;
		}

		return -1;

	} else if (d[0] == 0x1b && d[1] == 0x5b) {
		if (d[2] == 0x31) {
			if (d[3] >= 0x35 && d[3] <= 0x39) {
				r = keys_f5_f8[d[3] - 0x30];
				i = 4;
				goto I_3b_or_7e;

			} else if (d[3] == 0x3b) {
				if (d[4] >= 0x32 && d[4] <= 0x38) {
					r = (ffuint)keys_mod[d[4] - 0x30] << 24;
					i = 5;
					goto I_41_48;
				}
			}

			return -1;

		} else if (d[2] == 0x32
			&& d[3] >= 0x30 && d[3] <= 0x34) {
			r = keys_f9_f12[d[3] - 0x30];
			i = 4;
			goto I_3b_or_7e;

		} else if (d[2] >= 0x32 && d[2] <= 0x36) {
			r = keys_3x[d[2] - 0x30];
			i = 3;
			goto I_3b_or_7e;
		}

		i = 2;
		goto I_41_48;
	}

I_3b_or_7e:
	if (d[i] == 0x3b) {
		i++;
		if (d[i] >= 0x32 && d[i] <= 0x38) {
			r |= (ffuint)keys_mod[d[i] - 0x30] << 24;
			i++;
			if (d[i] == 0x7e)
				goto fin;
		}
		return -1;

	} else if (d[i] == 0x7e) {
		goto fin;
	}
	return -1;

I_41_48:
	if (d[i] >= 0x41 && d[i] <= 0x48) {
		r |= keys_4x[d[i] - 0x40];
		goto fin;
	}
	return -1;

fin:
	ffstr_shift(data, i + 1);
	return (r) ? FFKEY_VIRT | r : -1;
}


static inline int ffstd_attr(fffd fd, ffuint attr, ffuint val)
{
	if (attr == FFSTD_VTERM) {
		struct stat st;
		return !(!fstat(fd, &st)
			&& (st.st_mode & S_IFMT) == S_IFCHR);
	}

	struct termios t;
	if (0 != tcgetattr(fd, &t))
		return -1;

	if (attr & FFSTD_ECHO) {
		if (val & FFSTD_ECHO)
			t.c_lflag |= ECHO;
		else
			t.c_lflag &= ~ECHO;
	}

	if (attr & FFSTD_LINEINPUT) {
		if (val & FFSTD_LINEINPUT) {
			t.c_lflag |= ICANON;
		} else {
			t.c_lflag &= ~ICANON;
			t.c_cc[VTIME] = 0;
			t.c_cc[VMIN] = 1;
		}
	}

	tcsetattr(fd, TCSANOW, &t);
	return 0;
}

#endif


/** Read from stdin.
Windows: 'cap' must be >=4.
Return N of bytes read */
static ffssize ffstdin_read(void *buf, ffsize cap);

/** Write to stdout */
static ffssize ffstdout_write(const void *data, ffsize len);

/** Write to stderr */
static ffssize ffstderr_write(const void *data, ffsize len);

/** %-formatted output to stdout
NOT printf()-compatible (see ffs_formatv()) */
static inline ffssize ffstdout_fmt(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	ffstr s = {};
	ffsize cap = 0;
	ffsize r = ffstr_growfmtv(&s, &cap, fmt, args);
	va_end(args);
	if (r != 0)
		r = ffstdout_write(s.ptr, r);
	ffstr_free(&s);
	return r;
}

/** %-formatted output to stderr
NOT printf()-compatible (see ffs_formatv()) */
static inline ffssize ffstderr_fmt(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	ffstr s = {};
	ffsize cap = 0;
	ffsize r = ffstr_growfmtv(&s, &cap, fmt, args);
	va_end(args);
	if (r != 0)
		r = ffstderr_write(s.ptr, r);
	ffstr_free(&s);
	return r;
}

/** Write %-formatted text line to stdout */
#define fflog(fmt, ...)  (void) ffstdout_fmt(fmt "\n", ##__VA_ARGS__)

/** Write text line to stdout */
#define fflogz(sz)  (void) ffstdout_fmt("%s\n", sz)

/** Read keyboard event from terminal
fd: usually ffstdin
ev: initialize to {} on very first use
data: the data read from stdin
Return N of bytes read on success
  0 if queue is empty
  <0 on error */
static int ffstd_keyread(fffd fd, ffstd_ev *ev, ffstr *data);

/** Parse key received from terminal
Return enum FFKEY;  'data' is shifted by the number of bytes processed
  <0 on error */
static int ffstd_keyparse(ffstr *data);


/** Set attribute on a terminal
attr: enum FFSTD_ATTR
val: enum FFSTD_ATTR
Return !=0 on error */
static int ffstd_attr(fffd fd, ffuint attr, ffuint val);
