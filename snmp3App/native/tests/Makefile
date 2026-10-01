TOP = ../../..
include $(TOP)/configure/CONFIG

SRC_DIRS += $(TOP)/tests/rewrite
USR_INCLUDES += -I$(TOP)/snmp3App/src -I$(TOP)/snmp3App/native
NET_SNMP_CONFIG ?= net-snmp-config
NATIVE_CFLAGS = $(shell $(NET_SNMP_CONFIG) --cflags)
NATIVE_LIBS = $(shell $(NET_SNMP_CONFIG) --libs)

PROD_HOST += snmp3NativeAgent snmp3NativeApiProbe snmp3NativeTest
snmp3NativeAgent_SRCS += NativeAgent.cpp
NativeAgent_CPPFLAGS += $(NATIVE_CFLAGS)
snmp3NativeAgent_LDFLAGS += $(filter-out -l%,$(NATIVE_LIBS))
snmp3NativeAgent_SYS_LIBS += netsnmpagent $(patsubst -l%,%,$(filter -l%,$(NATIVE_LIBS)))
snmp3NativeApiProbe_SRCS += NativeApiProbe.cpp
NativeApiProbe_CPPFLAGS += $(NATIVE_CFLAGS)
snmp3NativeApiProbe_LDFLAGS += $(filter-out -l%,$(NATIVE_LIBS))
snmp3NativeApiProbe_SYS_LIBS += $(patsubst -l%,%,$(filter -l%,$(NATIVE_LIBS)))

snmp3NativeTest_SRCS += NativeTest.cpp
snmp3NativeTest_LIBS += snmp3Native snmp3 Com

include $(TOP)/configure/RULES
