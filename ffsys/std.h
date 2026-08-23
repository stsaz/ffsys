/** ffsys: standard I/O
2020, Simon Zolin */

/*
ffstd_info
ffstdin_read
ffstdout_write ffstderr_write
ffstdout_fmt ffstderr_fmt
fflog fflogz
ffstd_key_read
ffstd_key_parse
ffstd_attr
ffstd_paste_ctl ffstd_paste_read
*/

#pragma once
#include <ffsys/string.h>
#include <ffbase/unicode.h>

/** Terminal key code.
Up to 4-byte UTF-8 character or virtual key with optional modifiers.
Examples (uint32 little-endian):
	Byte3     Byte2     Byte1     Byte0
* Virtual key:
	0000VASC  00000000  00000000  0000xxxx
* UTF-8:
	00000ASC  00000000  00000000  0xxxxxxx
	00000ASC  00000000  10xxxxxx  110xxxxx
	00000ASC  10xxxxxx  10xxxxxx  1110xxxx
	10xxxxxx  10xxxxxx  10xxxxxx  11110xxx
Modifiers:
	* A=Alt
	* S=Shift
	* C=Ctrl
*/
enum FFKEY {
	FFKEY_BACKSPACE = 0x08,
	FFKEY_TAB = 0x09,
	FFKEY_ENTER = 0x0d,
	FFKEY_ESCAPE = 0x1b,

	FFKEY_VIRT = 0x08000000,
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
	FFKEY_TEXT_PASTED,

	FFKEY_CTRL = 0x01000000,
	FFKEY_SHIFT = 0x02000000,
	FFKEY_ALT = 0x04000000,
	FFKEY_MODMASK = FFKEY_CTRL | FFKEY_SHIFT | FFKEY_ALT,
};

enum FFSTD_ATTR {
	FFSTD_ECHO = 1, // set echo on/off (stdin)
	FFSTD_LINEINPUT = 2, // set line-input/character-input (stdin)
	FFSTD_VTERM = 4, // Windows: enable control character sequences
};

enum FFSTD_INFO {
	FFSTD_IN_ECHO = 1,
	FFSTD_IN_LINEINPUT = 2,

	FFSTD_OUT_VTERM = 1,
};

struct ffstd_info {
	ffuint input, output; // enum FFSTD_INFO
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

static inline int ffstd_info(fffd fd, struct ffstd_info *i)
{
	DWORD m;
	if (!GetConsoleMode(fd, &m))
		return -1;
	i->input = (m & ENABLE_ECHO_INPUT) ? FFSTD_IN_ECHO : 0;
	i->input |= (m & ENABLE_LINE_INPUT) ? FFSTD_IN_LINEINPUT : 0;
	i->output = (m & ENABLE_VIRTUAL_TERMINAL_PROCESSING) ? FFSTD_OUT_VTERM : 0;
	return 0;
}

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

static inline int ffstd_key_read(fffd fd, char *buf, ffsize cap)
{
	DWORD n;
	if (!GetNumberOfConsoleInputEvents(fd, &n))
		return -1;
	if (n == 0)
		return 0;
	if (!ReadConsoleInput(fd, (INPUT_RECORD*)buf, cap / sizeof(INPUT_RECORD), &n))
		return -1;
	return n * sizeof(INPUT_RECORD);
}

static inline int ffstd_key_parse_win(const char *data, ffsize len, ffuint *read)
{
	if (len < sizeof(INPUT_RECORD))
		return 0;
	const INPUT_RECORD *ir = (INPUT_RECORD*)data;
	*read = sizeof(INPUT_RECORD);
	if (!(ir->EventType == KEY_EVENT && ir->Event.KeyEvent.bKeyDown))
		return FFKEY_VIRT;

	const KEY_EVENT_RECORD *k = (KEY_EVENT_RECORD*)&ir->Event.KeyEvent;
	ffuint r = k->uChar.AsciiChar;
	if (r == 0) {
		if (k->wVirtualKeyCode >= VK_PRIOR && k->wVirtualKeyCode <= VK_DELETE) {
			static const ffbyte keys_vk[] = {
				FFKEY_PGUP & 0xff,	// VK_PRIOR
				FFKEY_PGDN & 0xff,	// VK_NEXT
				FFKEY_END & 0xff,	// VK_END
				FFKEY_HOME & 0xff,	// VK_HOME
				FFKEY_LEFT & 0xff,	// VK_LEFT
				FFKEY_UP & 0xff,	// VK_UP
				FFKEY_RIGHT & 0xff,	// VK_RIGHT
				FFKEY_DOWN & 0xff,	// VK_DOWN
				0,
				0,
				0,
				0,
				FFKEY_INS & 0xff,	// VK_INSERT
				FFKEY_DEL & 0xff,	// VK_DELETE
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
	if ((ctl & SHIFT_PRESSED) && !k->uChar.AsciiChar)
		r |= FFKEY_SHIFT;

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

static inline void ffstd_paste_ctl(ffuint enable)
{
	(void)enable;
}

static inline ffuint ffstd_paste_read(const char *d, ffsize len, ffstr *text)
{
	(void)d, (void)len, (void)text;
	return 0;
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

static inline int ffstd_key_read(fffd fd, char *buf, ffsize cap)
{
	ffssize r = read(fd, buf, cap);
	if (r < 0 && fferr_again(errno))
		return 0;
	else if (r == 0) {
		errno = EINVAL;
		return -1;
	}
	return r;
}

static inline int ffstd_info(fffd fd, struct ffstd_info *i)
{
	struct termios t;
	if (tcgetattr(fd, &t))
		return -1;
	i->input = (t.c_lflag & ECHO) ? FFSTD_IN_ECHO : 0;
	i->input |= (t.c_lflag & ICANON) ? FFSTD_IN_LINEINPUT : 0;
	i->output = FFSTD_OUT_VTERM;
	return 0;
}

static inline int ffstd_attr(fffd fd, ffuint attr, ffuint val)
{
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

static inline void ffstd_paste_ctl(ffuint enable)
{
	ffstdout_write((enable) ? "\x1b[?2004h" : "\x1b[?2004l", 8);
}

/**
Return N of bytes read;
  0 on error */
static inline ffuint ffstd_paste_read(const char *d, ffsize len, ffstr *text)
{
	ffssize r = ffs_findstr(d, len, "\x1b[201~", 6);
	if (r >= 0 && r < 6)
		return 0; // Incorrect usage
	text->ptr = (char*)d + 6;
	if (r < 0) {
		text->len = len - 6;
		return 0;
	}
	text->len = r - 6;
	return r + 6;
}

#endif

/** Get attributes of a terminal descriptor.
Return !=0 on error (fd is not a terminal) */
static int ffstd_info(fffd fd, struct ffstd_info *i);

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
		3030..3031 -> paste start/end
			7e
		30..34 -> F9..F12
			3b:
				32..38 -> Shift..SAC
					7e
			7e
	32..36 -> INS..PGDN
		3b:
			32..38 -> Shift..SAC
				7e
		7e
	41..48 -> UP..HOME
1b:
	XX|XXXX|XXXXXX -> Alt+key
*/
static inline int ffstd_key_parse_unix(const char *data, ffsize len, ffuint *read)
{
	int r = 0, i = 0;
	if (len == 0)
		return 0;
	if (data[0] != 0x1b) {
		r = ffutf8_decode(data, len, (ffuint*)&i);
		if (r < 0)
			return 0;
		else if (r == 0 || r > 4)
			return -1;
		*read = r;
		ffmem_copy(&i, data, r);
		return i;
	}

	ffbyte d[8] = {};
	ffmem_copy(d, data, ffmin(len, 8));

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
		0,
		0,
		0,
		0,
		0,
		FFKEY_F5 & 0xff,
		0,
		FFKEY_F6 & 0xff,
		FFKEY_F7 & 0xff,
		FFKEY_F8 & 0xff,
	};
	static const ffbyte keys_f9_f12[] = {
		FFKEY_F9 & 0xff,
		FFKEY_F10 & 0xff,
		0,
		FFKEY_F11 & 0xff,
		FFKEY_F12 & 0xff,
	};
	static const ffbyte keys_3x[] = {
		0,
		0,
		FFKEY_INS & 0xff,
		FFKEY_DEL & 0xff,
		0,
		FFKEY_PGUP & 0xff,
		FFKEY_PGDN & 0xff,
	};
	static const ffbyte keys_4x[] = {
		0,
		FFKEY_UP & 0xff,
		FFKEY_DOWN & 0xff,
		FFKEY_RIGHT & 0xff,
		FFKEY_LEFT & 0xff,
		0,
		FFKEY_END & 0xff,
		0,
		FFKEY_HOME & 0xff,
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

		} else if (d[2] == 0x32 && d[3] == 0x30
			&& (d[4] == 0x30 || d[4] == 0x31)
			&& d[5] == 0x7e) {
			*read = 6;
			return FFKEY_TEXT_PASTED;

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

	} else if (d[0] == 0x1b) {
		r = ffutf8_decode(data + 1, len - 1, (ffuint*)&i);
		if (r < 0)
			return 0;
		else if (r == 0 || r > 3)
			return -1;
		*read = 1 + r;
		ffmem_copy(&i, data + 1, r);
		return FFKEY_ALT | i;

	} else {
		return -1;
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
	*read = i + 1;
	return (r) ? FFKEY_VIRT | r : -1;
}

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
data: the output data read from stdin
Return N of bytes read on success
  0 if queue is empty
  <0 on error */
static int ffstd_key_read(fffd fd, char *buf, ffsize cap);

/** Parse key received from terminal
len: data length
read: N of bytes read
Windows: return FFKEY_VIRT for unhandled events (should be skipped or handled by user code).
Return enum FFKEY;
  0 if not enough data;
  -1 on error */
static inline int ffstd_key_parse(const char *data, ffsize len, ffuint *read)
{
#ifdef FF_WIN
	return ffstd_key_parse_win(data, len, read);
#endif
	return ffstd_key_parse_unix(data, len, read);
}


/** Set attribute on a terminal
attr: enum FFSTD_ATTR
val: enum FFSTD_ATTR
Return !=0 on error */
static int ffstd_attr(fffd fd, ffuint attr, ffuint val);

/** Set window title. */
static inline void ffstd_title(const char *title, ffsize len)
{
	char buf[5 + 255];
	len = ffmin(len, 255);
	int n = ffs_format(buf, sizeof(buf), "\x1b]0;%*s\x07", len, title);
	FF_ASSERT(n > 0);
	ffstdout_write(buf, n);
}
