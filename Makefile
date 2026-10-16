override CFLAGS += -Iinc -Wall -Wextra -Wpedantic -Wno-unused-function -fPIC

PREFIX ?= /usr/local

TARGET ?= $(shell uname)
LIBPREFIX ?= dst/$(TARGET)/lib

CC ?= cc
AR ?= ar

TARGET_CC := $(CROSS_COMPILE)$(CC)
TARGET_AR := $(CROSS_COMPILE)$(AR)

ifeq ($(TARGET),Windows)
	LDFLAGS += -lws2_32
	LIBPREFIX := dst/Windows/
endif

NAME ?= yaooi

SHARED := $(LIBPREFIX)$(NAME)
STATIC := $(LIBPREFIX)$(NAME).a
OBJECT := $(LIBPREFIX)$(NAME).o

ifeq ($(TARGET),Windows)
	SHARED := $(SHARED).dll
else
	SHARED := $(SHARED).so
endif

ifeq ($(DEBUG),yes)
	override CFLAGS += -g
else
	override CFLAGS += -O3
endif

.PHONY: sources flags all clean

all: $(SHARED) $(STATIC)
clean:
	rm -rf dst

install:
	mkdir -p $(PREFIX)/lib
	cp $(SHARED) $(PREFIX)/lib/

sources:
	echo $(SRCS)
flags:
	echo $(CFLAGS)

$(SHARED): $(OBJECT) | dst/$(TARGET)/
	$(TARGET_CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)

$(STATIC): $(OBJECT) | dst/$(TARGET)/
	$(TARGET_AR) rcs $@ $^

$(OBJECT): src/yaooi.c | dst/$(TARGET)/
	$(TARGET_CC) $(CFLAGS) -c $< -o $(OBJECT) -MMD

%/:
	mkdir -p $@

-include $(OBJECT:.o=.d)
