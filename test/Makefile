# ffsys makefile

ROOT := ../..
FFSYS_DIR := $(ROOT)/ffsys
FFBASE_DIR := $(ROOT)/ffbase

include $(FFBASE_DIR)/conf.mk

TEST_EXE := ffsystest$(DOTEXE)

OBJ := \
	test.o \
	compat.o \
	mem.o \
	error.o \
	\
	backtrace.o \
	dir.o \
	dylib.o \
	environ.o \
	file.o \
	filemap.o \
	kcall.o \
	kqueue.o \
	path.o \
	perf.o \
	pipe.o \
	process.o \
	semaphore.o \
	signal.o \
	std.o \
	thread.o \
	timer.o \
	timerqueue.o \
	\
	netconf.o \
	socket.o
# 	fileaio.o
ifeq "$(OS)" "windows"
	OBJ += \
		vol.o \
		filemon.o \
		winreg.o
else
	ifeq "$(OS)" "linux"
		OBJ += \
			filemon.o \
			netlink.o
	else ifeq "$(OS)" "freebsd"
	else ifeq "$(OS)" "apple"
	endif
endif

default: $(TEST_EXE)

clean:
	$(RM) $(TEST_EXE) $(OBJ)

CFLAGS := -MMD -MP \
	-I$(FFSYS_DIR) -I$(FFBASE_DIR) \
	-Wall -Wextra -Wno-array-bounds -fno-strict-aliasing \
	-DFFBASE_HAVE_FFERR_STR -DFF_DEBUG -g
ifeq "$(DEBUG)" "1"
	CFLAGS += -O0 -DFFBASE_DEBUG
else
	CFLAGS += -O3
endif
CXXFLAGS := $(CFLAGS)
CFLAGS += -std=gnu99

ifeq "$(ASAN)" "1"
	CFLAGS += -fsanitize=address
	LINKFLAGS += -fsanitize=address
endif

ifeq "$(OS)" "windows"
	# CFLAGS += -DFF_WIN_APIVER=0x0501
	LINKFLAGS += -lws2_32 -liphlpapi
else
	LINKFLAGS += -pthread
	ifeq "$(OS)" "linux"
		LINKFLAGS += -ldl
	else ifeq "$(OS)" "freebsd"
		LINKFLAGS += -lexecinfo
	endif
endif

-include $(wildcard *.d)

%.o: $(FFSYS_DIR)/ffsys/%.c
	$(C) $(CFLAGS) $< -o $@

%.o: $(FFSYS_DIR)/test/%.c
	$(C) $(CFLAGS) $< -o $@

%.o: $(FFSYS_DIR)/test/%.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

$(TEST_EXE): $(OBJ)
	$(LINK) $+ $(LINKFLAGS) -o $@
