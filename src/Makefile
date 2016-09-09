TOP = ..
include $(TOP)/configure/CONFIG

# Windows architectures are: win32-x86 and windows-x64

ifneq ($(EPICS_HOST_ARCH),win32-x86)
ifneq ($(EPICS_HOST_ARCH),windows-x64)
USR_CFLAGS += -Werror
USR_CFLAGS += -Wall
USR_CFLAGS += -Wbad-function-cast
USR_CFLAGS += -Wmissing-noreturn
USR_CFLAGS += -Wcast-qual
USR_CFLAGS += -Wcomment
USR_CFLAGS += -Wcomments
USR_CFLAGS += -Wformat
# USR_CFLAGS += -Wimplicit-function-dec
USR_CFLAGS += -Wimplicit-int
USR_CFLAGS += -Wimport
USR_CFLAGS += -Wmissing-braces
USR_CFLAGS += -Wmissing-declarations
USR_CFLAGS += -Wmissing-prototypes
USR_CFLAGS += -Wmultichar
USR_CFLAGS += -Wnested-externs
USR_CFLAGS += -Wparentheses
USR_CFLAGS += -Wpointer-arith
USR_CFLAGS += -Wsign-compare
USR_CFLAGS += -Wunknown-pragmas
USR_CFLAGS += -Wtrigraphs
USR_CFLAGS += -Wwrite-strings
endif
endif

LIBRARY_IOC += pvlogging

DBD += pvlogging.dbd

pvlogging_SRCS += pvlogging_registerRecordDeviceDriver.cpp
pvlogging_SRCS += pvlogging.c

DATA += access.acf


include $(TOP)/configure/RULES
