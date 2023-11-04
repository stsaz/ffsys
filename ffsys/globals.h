/** ffsys: define global symbols */

#include <ffsys/base.h>
int _ffcpu_features;

#ifdef FF_WIN

#include <ffsys/dylib.h>
signed char _ffdl_open_supports_flags;

#include <ffsys/error.h>
char _fferr_buffer[1024];

#include <ffsys/socket.h>
LPFN_DISCONNECTEX _ff_DisconnectEx;
LPFN_CONNECTEX _ff_ConnectEx;
LPFN_ACCEPTEX _ff_AcceptEx;
LPFN_GETACCEPTEXSOCKADDRS _ff_GetAcceptExSockaddrs;

#include <ffsys/perf.h>
LARGE_INTEGER _fftime_perffreq;
_ff_NtQuerySystemInformation_t _ff_NtQuerySystemInformation;
_ff_GetProcessMemoryInfo_t _ff_GetProcessMemoryInfo;

#else // UNIX:

#include <ffsys/environ.h>
char **_ff_environ;

#endif

#include <ffsys/signal.h>
ffsig_handler _ffsig_userhandler;
