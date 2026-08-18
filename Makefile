override CFLAGS += -Iinc -Wall -Wextra -Wno-unused-function -fPIC

TARGET ?= $(shell uname)
LIBPREFIX ?= bin/$(TARGET)/lib

CC ?= cc
AR ?= ar

TARGET_CC := $(CROSS_COMPILE)$(CC)
TARGET_AR := $(CROSS_COMPILE)$(AR)

ifeq ($(TARGET),Windows)
	LDFLAGS += -lws2_32
	LIBPREFIX := bin/Windows/
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
endif

.PHONY: sources flags all clean

all: $(SHARED) $(STATIC)
clean:
	rm -rf bin

sources:
	echo $(SRCS)
flags:
	echo $(CFLAGS)

$(SHARED): $(OBJECT) | bin/$(TARGET)/
	$(TARGET_CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)

$(STATIC): $(OBJECT) | bin/$(TARGET)/
	$(TARGET_AR) rcs $@ $^

$(OBJECT): src/yaooi.c | bin/$(TARGET)/
	$(TARGET_CC) $(CFLAGS) -c $< -o $(OBJECT) -MMD

%/:
	mkdir -p $@

-include $(OBJECT:.o=.d)
