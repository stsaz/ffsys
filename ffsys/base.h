/** ffsys: detect CPU, OS and compiler; base types; heap memory functions
2020, Simon Zolin */

#ifndef _FFSYS_BASE_H
#define _FFSYS_BASE_H

#include <ffsys/detect-os.h>

#if defined FF_LINUX
	#ifndef _GNU_SOURCE
		#define _GNU_SOURCE
	#endif

	#ifndef _POSIX_C_SOURCE
		#define _POSIX_C_SOURCE  200112L // for posix_memalign()
	#endif

	#ifndef _LARGEFILE64_SOURCE
		#define _LARGEFILE64_SOURCE 1
	#endif
#endif

#if defined FF_UNIX
	#include <stdlib.h>
	#include <unistd.h>
	#include <string.h>
	#include <errno.h>

	typedef int fffd;
	typedef char ffsyschar;

#else // Windows:

	#define _WIN32_WINNT FF_WIN_APIVER
	#define NOMINMAX
	#define _CRT_SECURE_NO_WARNINGS
	#define _CRT_RAND_S
	#define OEMRESOURCE //gui
	#include <winsock2.h>
	#include <ws2tcpip.h>

	typedef HANDLE fffd;
	typedef wchar_t ffsyschar;

#endif

#include <ffbase/base.h>
#include <ffsys/detect-compiler.h>

typedef signed char ffint8;
typedef int ffbool;

#endif
