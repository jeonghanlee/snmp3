TOP = ../..
include $(TOP)/configure/CONFIG

LIBRARY_IOC += snmp3
LIBRARY_IOC += snmp3Wire
snmp3Wire_SRCS += Ipc.cpp Identity.cpp
snmp3_SRCS += Register.cpp Runtime.cpp Config.cpp Json.cpp Scheduler.cpp Supervisor.cpp
snmp3_SRCS += Conversion.cpp Request.cpp DeviceSupport.cpp
snmp3_LIBS += snmp3Wire dbCore Com

DBD += snmp3.dbd snmp3Ioc.dbd
snmp3Ioc_DBD += base.dbd snmp3.dbd

PROD_IOC += snmp3Ioc
snmp3Ioc_SRCS += Main.cpp snmp3Ioc_registerRecordDeviceDriver.cpp
snmp3Ioc_LIBS += snmp3 $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
