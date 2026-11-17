# Independent Base 7.0.10 application build.
TOP = .
include $(TOP)/configure/CONFIG

DIRS += configure snmp3App
snmp3App_DEPEND_DIRS += configure

include $(TOP)/configure/RULES_TOP

# The mibs directory is installed by snmp3App/mibs and removed with the other install directories.
UNINSTALL_DIRS += $(INSTALL_LOCATION)/mibs
