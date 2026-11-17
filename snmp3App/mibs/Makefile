TOP = ../..
include $(TOP)/configure/CONFIG

# The mibs directory of the module: MIB files that every IOC of this installation finds, after the places the
# IOC names with snmp3Mib and before the directories of the system. The module ships a note and no MIB file.
INSTALL_MIBS = $(INSTALL_LOCATION)/mibs
MIBS += README

include $(TOP)/configure/RULES

buildInstall: $(MIBS:%=$(INSTALL_MIBS)/%)

$(INSTALL_MIBS)/%: ../%
	$(ECHO) "Installing MIB directory file $@"
	@$(INSTALL) -d -m 644 $< $(@D)
