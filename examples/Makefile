# ffsys: Makefile for example code

ROOT_DIR := ../..
FFBASE_DIR := $(ROOT_DIR)/ffbase
FFSYS_DIR := $(ROOT_DIR)/ffsys
DIR := $(FFSYS_DIR)/examples
LINKER := gcc
CFLAGS := -I$(FFSYS_DIR) -I$(FFBASE_DIR)
CFLAGS += -O0 -g

all: tcp-echo-server \
	tcp-msg-router

%: $(DIR)/%.c
	$(LINKER) $(CFLAGS) $+ -o $@
