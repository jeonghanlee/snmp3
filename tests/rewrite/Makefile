TOP = ../..
include $(TOP)/configure/CONFIG

USR_INCLUDES += -I$(TOP)/snmp3App/src
USR_DBDFLAGS += -I$(TOP)/dbd
DBD += snmp3LifecycleTest.dbd
snmp3LifecycleTest_DBD += base.dbd snmp3.dbd
PROD_IOC += snmp3LifecycleTest
snmp3LifecycleTest_SRCS += LifecycleTest.cpp snmp3LifecycleTest_registerRecordDeviceDriver.cpp
snmp3LifecycleTest_LIBS += snmp3 $(EPICS_BASE_IOC_LIBS)

PROD_HOST += snmp3ConfigTest
snmp3ConfigTest_SRCS += ConfigTest.cpp
snmp3ConfigTest_LIBS += snmp3 Com

PROD_HOST += snmp3ConversionTest
snmp3ConversionTest_SRCS += ConversionTest.cpp
snmp3ConversionTest_LIBS += snmp3 Com

PROD_HOST += snmp3IpcTest
snmp3IpcTest_SRCS += IpcTest.cpp
snmp3IpcTest_LIBS += snmp3Wire

PROD_HOST += snmp3SchedulerTest
snmp3SchedulerTest_SRCS += SchedulerTest.cpp
snmp3SchedulerTest_LIBS += snmp3 snmp3Wire Com

PROD_HOST += snmp3InventoryTest
snmp3InventoryTest_SRCS += InventoryTest.cpp
InventoryTest_CPPFLAGS += $(shell net-snmp-config --cflags)
USR_INCLUDES += -I$(TOP)/snmp3App/native
snmp3InventoryTest_LIBS += snmp3 snmp3Wire Com

PROD_HOST += snmp3SupervisorTest
snmp3SupervisorTest_SRCS += SupervisorTest.cpp
snmp3SupervisorTest_LIBS += snmp3 snmp3Wire Com

PROD_HOST += snmp3QualificationTest
snmp3QualificationTest_SRCS += QualificationTest.cpp
snmp3QualificationTest_LIBS += snmp3 snmp3Wire Com

DBD += snmp3RuntimeTest.dbd
snmp3RuntimeTest_DBD += base.dbd snmp3.dbd snmp3RuntimeTestRegistrar.dbd
PROD_IOC += snmp3RuntimeTest
snmp3RuntimeTest_SRCS += RuntimeTest.cpp snmp3RuntimeTest_registerRecordDeviceDriver.cpp
snmp3RuntimeTest_LIBS += snmp3 snmp3Wire $(EPICS_BASE_IOC_LIBS)

DBD += snmp3RecordTest.dbd
snmp3RecordTest_DBD += base.dbd snmp3.dbd
PROD_IOC += snmp3RecordTest
snmp3RecordTest_SRCS += RecordTest.cpp snmp3RecordTest_registerRecordDeviceDriver.cpp
snmp3RecordTest_LIBS += snmp3 snmp3Wire $(EPICS_BASE_IOC_LIBS)

SRC_DIRS += $(TOP)/snmp3App/src
DBD += snmp3ShutdownTest.dbd
snmp3ShutdownTest_DBD += base.dbd snmp3.dbd snmp3ShutdownTestRegistrar.dbd
PROD_IOC += snmp3ShutdownTest
snmp3ShutdownTest_SRCS += Main.cpp ShutdownTest.cpp snmp3ShutdownTest_registerRecordDeviceDriver.cpp
snmp3ShutdownTest_LIBS += snmp3 snmp3Wire $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
