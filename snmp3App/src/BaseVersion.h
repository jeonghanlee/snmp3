#ifndef SNMP3_BASE_VERSION_H
#define SNMP3_BASE_VERSION_H

#include <epicsVersion.h>

#if EPICS_VERSION != 7 || EPICS_REVISION != 0 || EPICS_MODIFICATION != 10
#  error "snmp3 requires EPICS Base 7.0.10"
#endif

#endif
