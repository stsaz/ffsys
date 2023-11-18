# ffsys

ffsys is a cross-platform interface that wraps the system functions of different OS to provide you with a unified userspace API.
In other words, you write the single chunk of code that is translated to system-specific code during compilation, and that's how it works on different OS.

Supported OS:

* Linux and Android
* FreeBSD
* macOS
* Windows

ffsys advantages:

* No external dependencies - built upon `libc` and `kernel32.dll`
* No overhead - everything is inlined during the compiler's optimization
* Header-only - it's convenient to just `#include` the necessary files directly into your C/C++ source file
* Clean and straightforward code that you can copy-paste anywhere


## Features

Main:

	base.h          Detect CPU, OS and compiler; base types; heap memory functions
	globals.h       Define the global symbols needed by ffsys.
	                Include this into one of your compilation units to make the linker happy.

I/O, FS:

	std.h           Standard I/O
	file.h          Files
	filemap.h       File mapping
	pipe.h          Unnamed and named pipes
	queue.h         Kernel queue
	kcall.h         Kernel call queue (to call kernel functions asynchronously)
	dir.h           File-system directory functions
	dirscan.h       Scan directory for files
	path.h          Native file-system paths
	filemon.h       File system monitoring (Linux, Windows) (incompatible)
	volume.h        Volumes (Windows)
	winreg.h        Windows Registry

Process, thread, IPC:

	process.h       Process
	environ.h       Process environment
	thread.h        Threads
	signal.h        UNIX signals, CPU exceptions
	semaphore.h     Semaphores
	perf.h          Process/thread performance counters
	dylib.h         Dynamically loaded libraries
	backtrace.h     Backtrace

Network:

	socket.h        Sockets, network address
	netconf.h       Network configuration
	netlink.h       Linux netlink helper functions

Misc:

	time.h          Date and time
	timer.h         System timer
	timerqueue.h    Timer queue
	string.h        Native system string
	error.h         System error codes
	random.h        Random number generator


## How to use

ffsys code is split between 2 repos:

* [ffbase](https://github.com/stsaz/ffbase) library that provides base types & algorithms for C.  It is also header-only.
* This repo contains system function wrappers.  It uses `ffbase`, e.g. for string manipulation and Unicode conversion functions.

Step 1. In your terminal window:

```sh
cd YOUR_PROJECT
git clone https://github.com/stsaz/ffsys
git clone https://github.com/stsaz/ffbase
```

Step 2. In your Makefile:

	FFSYS := YOUR_PROJECT/ffsys
	FFBASE := YOUR_PROJECT/ffbase
	CFLAGS += -I$(FFSYS) -I$(FFBASE)

Optional preprocessor flags:

| Flag      | Description |
| ---       | --- |
| `FF_MUSL` | UNIX: Compile for musl C library |

Step 3. In your C source files:

```C
#include <ffsys/...>
```


## Test

```sh
git clone https://github.com/stsaz/ffbase
git clone https://github.com/stsaz/ffsys
cd ffsys/test
make -j8
./ffsystest all
```

on FreeBSD:

	gmake -j8

on Linux for Windows:

	make -j8 OS=windows COMPILER=gcc CROSS_PREFIX=x86_64-w64-mingw32-
