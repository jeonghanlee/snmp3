TOP = ../..
include $(TOP)/configure/CONFIG

NET_SNMP_CONFIG ?= net-snmp-config
NATIVE_CFLAGS = $(shell $(NET_SNMP_CONFIG) --cflags)
NATIVE_LIBS = $(shell $(NET_SNMP_CONFIG) --libs)
USR_INCLUDES += -I$(TOP)/snmp3App/src

LIBRARY_HOST += snmp3Native
snmp3Native_SRCS += Capabilities.cpp Native.cpp Worker.cpp
Capabilities_CPPFLAGS += $(NATIVE_CFLAGS)
Native_CPPFLAGS += $(NATIVE_CFLAGS)
Worker_CPPFLAGS += $(NATIVE_CFLAGS)
snmp3Native_LDFLAGS += $(filter-out -l%,$(NATIVE_LIBS))
snmp3Native_SYS_LIBS += $(patsubst -l%,%,$(filter -l%,$(NATIVE_LIBS)))

PROD_HOST += snmp3NativeProbe
snmp3NativeProbe_SRCS += NativeProbe.cpp
NativeProbe_CPPFLAGS += $(NATIVE_CFLAGS)
snmp3NativeProbe_LDFLAGS += $(filter-out -l%,$(NATIVE_LIBS))
snmp3NativeProbe_SYS_LIBS += $(patsubst -l%,%,$(filter -l%,$(NATIVE_LIBS)))
snmp3NativeProbe_LIBS += snmp3Native Com

PROD_HOST += snmp3Worker
snmp3Worker_SRCS += WorkerMain.cpp
snmp3Worker_LIBS += snmp3Native snmp3Wire Com

include $(TOP)/configure/RULES
