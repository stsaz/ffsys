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

| File | Description |
| --- | --- |
| [base.h](ffsys/base.h)       | Detect CPU, OS and compiler; base types; heap memory functions |
| [globals.h](ffsys/globals.h) | Define the global symbols needed by ffsys. Include this file into one of your compilation units to make the linker happy. |

I/O, FS:

| File | Description |
| --- | --- |
| [std.h](ffsys/std.h)         | Standard I/O |
| [file.h](ffsys/file.h)       | Files |
| [filemap.h](ffsys/filemap.h) | File mapping |
| [pipe.h](ffsys/pipe.h)       | Unnamed and named pipes |
| [queue.h](ffsys/queue.h)     | Kernel queue |
| [kcall.h](ffsys/kcall.h)     | Kernel call queue (to call kernel functions asynchronously) |
| [dir.h](ffsys/dir.h)         | File-system directory functions |
| [dirscan.h](ffsys/dirscan.h) | Scan directory for files |
| [path.h](ffsys/path.h)       | Native file-system paths |
| [filemon.h](ffsys/filemon.h) | File system monitoring (Linux, Windows) (incompatible) |
| [volume.h](ffsys/volume.h)   | Volumes (Windows) |
| [winreg.h](ffsys/winreg.h)   | Windows Registry |

Process, thread, IPC:

| File | Description |
| --- | --- |
| [process.h](ffsys/process.h)     | Process |
| [environ.h](ffsys/environ.h)     | Process environment |
| [thread.h](ffsys/thread.h)       | Threads |
| [signal.h](ffsys/signal.h)       | UNIX signals, CPU exceptions |
| [semaphore.h](ffsys/semaphore.h) | Semaphores |
| [perf.h](ffsys/perf.h)           | Process/thread performance counters |
| [dylib.h](ffsys/dylib.h)         | Dynamically loaded libraries |
| [backtrace.h](ffsys/backtrace.h) | Backtrace |

Network:

| File | Description |
| --- | --- |
| [socket.h](ffsys/socket.h)   | Sockets, network address |
| [netconf.h](ffsys/netconf.h) | Network configuration |
| [netlink.h](ffsys/netlink.h) | Linux netlink helper functions |

Misc:

| File | Description |
| --- | --- |
| [time.h](ffsys/time.h)             | Date and time |
| [timer.h](ffsys/timer.h)           | System timer |
| [timerqueue.h](ffsys/timerqueue.h) | Timer queue |
| [string.h](ffsys/string.h)         | Native system string |
| [error.h](ffsys/error.h)           | System error codes |
| [random.h](ffsys/random.h)         | Random number generator |


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

```Makefile
FFSYS := YOUR_PROJECT/ffsys
FFBASE := YOUR_PROJECT/ffbase
CFLAGS += -I$(FFSYS) -I$(FFBASE)
```

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
