/** ffsys: thread
2020, Simon Zolin */

/*
ffthread_create
ffthread_join ffthread_detach
ffthread_sleep
ffthread_curid ffthread_current
ffthread_cpumask_set
ffthread_affinity
*/

#pragma once
#include <ffsys/base.h>

#ifdef FF_WIN

typedef HANDLE ffthread;
#define FFTHREAD_NULL  NULL

#define FFTHREAD_PROCCALL  __stdcall
typedef int (FFTHREAD_PROCCALL *ffthread_proc)(void *);

#define FFTHREAD_ETIMEDOUT  WSAETIMEDOUT

static inline ffthread ffthread_create(ffthread_proc proc, void *param, ffsize stack_size)
{
	return CreateThread(NULL, stack_size, (PTHREAD_START_ROUTINE)proc, param, 0, NULL);
}

static inline int ffthread_join(ffthread t, ffuint timeout_ms, int *exit_code)
{
	int r = WaitForSingleObject(t, timeout_ms);

	if (r == WAIT_OBJECT_0) {

		if (exit_code != NULL)
			GetExitCodeThread(t, (DWORD*)exit_code);

		CloseHandle(t);
		return 0;

	} else if (r == WAIT_TIMEOUT) {
		SetLastError(FFTHREAD_ETIMEDOUT);
	}

	return -1;
}

static inline int ffthread_detach(ffthread t)
{
	return 0 == CloseHandle(t);
}

typedef struct ffthread_cpumask ffthread_cpumask;
struct ffthread_cpumask {
	ffsize value;
};

static inline void ffthread_cpumask_set(ffthread_cpumask *mask, ffuint i)
{
	mask->value |= ((ffsize)1 << i);
}

static inline int ffthread_affinity(ffthread t, const ffthread_cpumask *mask)
{
	return (0 == SetThreadAffinityMask(t, mask->value));
}

static inline int ffthread_sleep(ffuint msec)
{
	Sleep(msec);
	return 0;
}

static inline ffthread ffthread_current()
{
	return GetCurrentThread();
}

static inline ffuint64 ffthread_curid()
{
	return GetCurrentThreadId();
}

#else // unix:

#include <pthread.h>
#include <errno.h>

#if defined FF_LINUX
	#include <time.h>
	#if !defined FF_ANDROID
		#define FFSYS_HAVE_pthread_timedjoin_np
	#endif
	#include <sys/syscall.h>
	typedef cpu_set_t ffthread_cpumask;

#elif defined FF_BSD
	#define FFSYS_HAVE_pthread_timedjoin_np
	#include <pthread_np.h>
	typedef cpuset_t ffthread_cpumask;

#endif

typedef pthread_t ffthread;
#define FFTHREAD_NULL  0

#define FFTHREAD_PROCCALL
typedef int (FFTHREAD_PROCCALL *ffthread_proc)(void *);

#define FFTHREAD_ETIMEDOUT  ETIMEDOUT

static inline ffthread ffthread_create(ffthread_proc proc, void *param, ffsize stack_size)
{
	ffthread t = 0;
	typedef void* (*start_routine) (void*);
	pthread_attr_t attr;
	pthread_attr_t *pattr = NULL;

	if (stack_size != 0) {
		if (0 != pthread_attr_init(&attr))
			return 0;
		pattr = &attr;
		if (0 != pthread_attr_setstacksize(&attr, stack_size))
			goto end;
	}

	if (0 != pthread_create(&t, pattr, (start_routine)(void*)proc, param))
		t = 0;

end:
	if (pattr != NULL)
		pthread_attr_destroy(pattr);

	return t;
}

/** Add msec value to 'timespec' object */
static inline void _fftimespec_addms(struct timespec *ts, ffuint64 ms)
{
	ts->tv_sec += ms / 1000;
	ts->tv_nsec += (ms % 1000) * 1000 * 1000;
	if (ts->tv_nsec >= 1000 * 1000 * 1000) {
		ts->tv_sec++;
		ts->tv_nsec -= 1000 * 1000 * 1000;
	}
}

static inline int ffthread_join(ffthread t, ffuint timeout_ms, int *exit_code)
{
	void *result;
	int r;

	if (timeout_ms == (ffuint)-1) {
		r = pthread_join(t, &result);
	}

#if defined FF_LINUX && !defined FF_ANDROID

	else if (timeout_ms == 0) {
		r = pthread_tryjoin_np(t, &result);
		if (r == EBUSY)
			r = FFTHREAD_ETIMEDOUT;
	}

#endif

#ifdef FFSYS_HAVE_pthread_timedjoin_np

	else {
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		_fftimespec_addms(&ts, timeout_ms);
		r = pthread_timedjoin_np(t, &result, &ts);
	}

#else
	else {
		r = pthread_join(t, &result);
	}
#endif

	if (r != 0) {
		errno = r;
		return -1;
	}

	if (exit_code != NULL)
		*exit_code = (int)(ffsize)result;
	return 0;
}

static inline ffthread ffthread_current()
{
	return pthread_self();
}

static inline ffuint64 ffthread_curid()
{
#if defined FF_LINUX

	return syscall(SYS_gettid);

#elif defined FF_APPLE

	ffuint64 tid;
	if (0 != pthread_threadid_np(NULL, &tid))
		return 0;
	return tid;

#elif defined FF_BSD

	return pthread_getthreadid_np();

#endif
}

static inline int ffthread_detach(ffthread t)
{
	return pthread_detach(t);
}

static inline void ffthread_cpumask_set(ffthread_cpumask *mask, ffuint i)
{
	CPU_SET(i, mask);
}

static inline int ffthread_affinity(ffthread t, const ffthread_cpumask *mask)
{
#ifdef FF_ANDROID
	errno = ENOSYS;
	return -1;

#else
	return pthread_setaffinity_np(t, sizeof(*mask), mask);
#endif
}

static inline int ffthread_sleep(ffuint msec)
{
	struct timespec ts = {
		.tv_sec = msec / 1000,
		.tv_nsec = (msec % 1000) * 1000000,
	};
	return nanosleep(&ts, NULL);
}

#endif

/** Create a thread
@stack_size: 0=default
Return FFTHREAD_NULL on error */
static ffthread ffthread_create(ffthread_proc proc, void *param, ffsize stack_size);

/** Join with the thread
timeout_ms: -1: infinite
  APPLE, ANDROID: values >=0 are treated as -1
exit_code: optional
Return
  0 if thread exited;
  !=0 on error, errno is set to:
    FFTHREAD_ETIMEDOUT if time has expired */
static int ffthread_join(ffthread t, ffuint timeout_ms, int *exit_code);

/** Detach thread */
static int ffthread_detach(ffthread t);

/** Add CPU number to mask. */
static void ffthread_cpumask_set(ffthread_cpumask *mask, ffuint i);

/** Set CPU affinity. */
static int ffthread_affinity(ffthread t, const ffthread_cpumask *mask);

/** Get the current thread descriptor. */
static ffthread ffthread_current();

/** Get ID of the current thread */
static ffuint64 ffthread_curid();

/** Suspend the thread for the specified time */
static int ffthread_sleep(ffuint msec);
