/** ffsys: kernel call queue
2022, Simon Zolin */

/*
ffkcallq_process_sq
ffkcallq_process_cq
ffkcall_cancel
fffile_open_async
fffile_info_async
fffile_read_async fffile_readat_async
fffile_write_async fffile_writeat_async
*/

#pragma once
#include <ffsys/file.h>
#include <ffsys/semaphore.h>
#include <ffsys/queue.h>
#include <ffsys/socket.h>
#include <ffsys/error.h>
#include <ffbase/ringqueue.h>
#include <assert.h>

#ifdef FF_WIN
#define FFKCALL_EINPROGRESS  ERROR_IO_PENDING
#define FFKCALL_EBUSY  ERROR_BUSY
#define FFKCALL_EAGAIN  WSAEWOULDBLOCK
#else
#define FFKCALL_EINPROGRESS  EINPROGRESS
#define FFKCALL_EBUSY  EBUSY
#define FFKCALL_EAGAIN  EAGAIN
#endif

struct ffkcallqueue {
	ffringqueue *sq; // submission queue
	ffsem sem; // triggerred on sq submit to wake sq reader thread (optional)

	ffringqueue *cq; // completion queue
	ffkq_postevent kqpost; // triggerred on cq submit to wake cq reader thread (optional)
	void *kqpost_data;
};

typedef void (*kcall_func)(void *obj);

struct ffkcall {
	struct ffkcallqueue *q;

	kcall_func handler;
	void *param;

	ffushort op; // enum FFKCALL_OP
	ffushort state; // 0, 1: in SQ, 2: in CQ
	union {
		struct {
			ffuint error;
			ffint64 result;
		};
		struct {
			ffuint flags;
			const char *name;
		};
		struct {
			fffd fd_info;
			fffileinfo *finfo;
		};
		struct {
			fffd	fd;
			void*	buf;
			ffsize	size;
			ffuint64 offset;
		};
	};
};

enum FFKCALL_OP {
	FFKCALL_FILE_OPEN = 1,
	FFKCALL_FILE_INFO,
	FFKCALL_FILE_READ,
	FFKCALL_FILE_READAT,
	FFKCALL_FILE_WRITE,
	FFKCALL_FILE_WRITEAT,
	FFKCALL_NET_RESOLVE,
};

static int _ffkcall_exec(struct ffkcall *kc)
{
	switch (kc->op) {
	case FFKCALL_FILE_OPEN:
		kc->result = (ffssize)fffile_open(kc->name, kc->flags);
		break;

	case FFKCALL_FILE_INFO:
		kc->result = fffile_info(kc->fd, kc->finfo);
		break;

	case FFKCALL_FILE_READ:
		kc->result = fffile_read(kc->fd, kc->buf, kc->size);
		break;

	case FFKCALL_FILE_READAT:
		kc->result = fffile_readat(kc->fd, kc->buf, kc->size, kc->offset);
		break;

	case FFKCALL_FILE_WRITE:
		kc->result = fffile_write(kc->fd, kc->buf, kc->size);
		break;

	case FFKCALL_FILE_WRITEAT:
		kc->result = fffile_writeat(kc->fd, kc->buf, kc->size, kc->offset);
		break;

	case FFKCALL_NET_RESOLVE:
		kc->result = (ffsize)ffaddrinfo_resolve(kc->name, kc->flags);
		break;

	default:
		return -1;
	}

	kc->error = fferr_last();
	return 0;
}

/** Process the submission queue, perform operations, store result in completion queue */
static inline void ffkcallq_process_sq(ffringqueue *sq)
{
	struct ffkcall *kc;
	for (;;) {
		if (0 != ffrq_fetch(sq, (void**)&kc, NULL))
			break;

		if (ff_unlikely(0 != _ffkcall_exec(kc))) {
			kc->state = 0;
			continue;
		}
		kc->state = 2;

		ffuint used;
		if (ff_unlikely(0 != ffrq_add(kc->q->cq, kc, &used))) {
			assert(0);
			continue;
		}

		if (used == 0 && kc->q->kqpost != FFKQ_NULL) {
			if (ff_unlikely(0 != ffkq_post(kc->q->kqpost, kc->q->kqpost_data)))
				assert(0);
		}
	}
}

/** Process the completion queue, call a result handling function */
static inline void ffkcallq_process_cq(ffringqueue *cq)
{
	struct ffkcall *kc;
	for (;;) {
		if (0 != ffrq_fetch_sr(cq, (void**)&kc, NULL))
			break;

		kc->state = 0;
		if (kc->op != 0)
			kc->handler(kc->param);
	}
}

static inline void ffkcall_cancel(struct ffkcall *kc)
{
	kc->op = 0;
}

static inline void _ffkcall_add(struct ffkcall *kc, int op)
{
	kc->op = op;
	ffuint used;
	if (0 != ffrq_add(kc->q->sq, kc, &used)) {
		kc->op = 0;
		fferr_set(FFKCALL_EAGAIN);
		return;
	}
	if (kc->q->sem != FFSEM_NULL) {
		ffsem_post(kc->q->sem);
	}
	fferr_set(FFKCALL_EINPROGRESS);
}

static int _ffkcall_busy(struct ffkcall *kc)
{
	if (kc->state != 0) {
		// the previous operation is still active in SQ or CQ
		fferr_set(FFKCALL_EBUSY);
		return 1;
	}
	return 0;
}

static int _ffkcall_complete(struct ffkcall *kc)
{
	if (kc->op != 0) {
		kc->op = 0;
		fferr_set(kc->error);
		return 1;
	}
	return 0;
}

static inline fffd fffile_open_async(const char *name, ffuint flags, struct ffkcall *kc)
{
	if (kc->q == NULL)
		return fffile_open(name, flags);

	if (_ffkcall_busy(kc))
		return FFFILE_NULL;

	if (_ffkcall_complete(kc))
		return (fffd)(ffssize)kc->result;

	kc->name = name;
	kc->flags = flags;
	_ffkcall_add(kc, FFKCALL_FILE_OPEN);
	return FFFILE_NULL;
}

static inline int fffile_info_async(fffd fd, fffileinfo *fi, struct ffkcall *kc)
{
	if (kc->q == NULL)
		return fffile_info(fd, fi);

	if (_ffkcall_busy(kc))
		return -1;

	if (_ffkcall_complete(kc))
		return kc->result;

	kc->fd = fd;
	kc->finfo = fi;
	_ffkcall_add(kc, FFKCALL_FILE_INFO);
	return -1;
}

static inline ffssize fffile_read_async(fffd fd, void *buf, ffsize size, struct ffkcall *kc)
{
	if (kc->q == NULL)
		return fffile_read(fd, buf, size);

	if (_ffkcall_busy(kc))
		return -1;

	if (_ffkcall_complete(kc))
		return kc->result;

	kc->fd = fd;
	kc->buf = buf;
	kc->size = size;
	_ffkcall_add(kc, FFKCALL_FILE_READ);
	return -1;
}

static inline ffssize fffile_readat_async(fffd fd, void *buf, ffsize size, ffuint64 offset, struct ffkcall *kc)
{
	if (kc->q == NULL)
		return fffile_readat(fd, buf, size, offset);

	if (_ffkcall_busy(kc))
		return -1;

	if (_ffkcall_complete(kc))
		return kc->result;

	kc->fd = fd;
	kc->buf = buf;
	kc->size = size;
	kc->offset = offset;
	_ffkcall_add(kc, FFKCALL_FILE_READAT);
	return -1;
}

static inline ffssize fffile_write_async(fffd fd, const void *buf, ffsize size, struct ffkcall *kc)
{
	if (kc->q == NULL)
		return fffile_write(fd, buf, size);

	if (_ffkcall_busy(kc))
		return -1;

	if (_ffkcall_complete(kc))
		return kc->result;

	kc->fd = fd;
	kc->buf = (void*)buf;
	kc->size = size;
	_ffkcall_add(kc, FFKCALL_FILE_WRITE);
	return -1;
}

static inline ffssize fffile_writeat_async(fffd fd, const void *buf, ffsize size, ffuint64 offset, struct ffkcall *kc)
{
	if (kc->q == NULL)
		return fffile_writeat(fd, buf, size, offset);

	if (_ffkcall_busy(kc))
		return -1;

	if (_ffkcall_complete(kc))
		return kc->result;

	kc->fd = fd;
	kc->buf = (void*)buf;
	kc->size = size;
	kc->offset = offset;
	_ffkcall_add(kc, FFKCALL_FILE_WRITEAT);
	return -1;
}

static inline ffaddrinfo* ffaddrinfo_resolve_async(const char *name, int flags, struct ffkcall *kc)
{
	if (kc->q == NULL)
		return ffaddrinfo_resolve(name, flags);

	if (_ffkcall_busy(kc))
		return NULL;

	if (_ffkcall_complete(kc))
		return (ffaddrinfo*)kc->result;

	kc->name = name;
	kc->flags = flags;
	_ffkcall_add(kc, FFKCALL_NET_RESOLVE);
	return NULL;
}
