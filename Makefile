# Independent Base 7.0.10 application build.
TOP = .
include $(TOP)/configure/CONFIG

DIRS += configure snmp3App
snmp3App_DEPEND_DIRS += configure

include $(TOP)/configure/RULES_TOP
