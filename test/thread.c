/** ffsys: thread.h tester
2020, Simon Zolin */

#include <ffsys/thread.h>
#include <ffsys/test.h>

static int FFTHREAD_PROCCALL thdfunc(void *param)
{
	xieq(0x12345, (ffsize)param);
	fflog("tid: %u", (int)ffthread_curid());
	ffthread_sleep(1000);
	return 1234;
}

void test_thread()
{
	ffthread th = ffthread_create(&thdfunc, (void*)0x12345, 64 * 1024);
	x_sys(th != FFTHREAD_NULL);

	ffthread_cpumask cs = {};
	ffthread_cpumask_set(&cs, 0);
	x_sys(!ffthread_affinity(th, &cs));

	fflog("waiting for thread...");
	int code;
	x(0 == ffthread_join(th, -1, &code));
	xieq(1234, code);

	ffthread_detach(th); // noop

	x_sys(FFTHREAD_NULL != (th = ffthread_create(&thdfunc, (void*)0x12345, 0)));
	fflog("waiting for thread...");

#ifdef FF_APPLE
	x(0 == ffthread_join(th, 0, &code));
	xieq(1234, code);

	x_sys(FFTHREAD_NULL != (th = ffthread_create(&thdfunc, (void*)0x12345, 0)));

#else
	x(0 != ffthread_join(th, 0, &code));
	xint_sys(FFERR_TIMEOUT, fferr_last());
#endif

	fflog("waiting for thread...");
	x(0 == ffthread_join(th, 2000, &code));
	xieq(1234, code);
}
