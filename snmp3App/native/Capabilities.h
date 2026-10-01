#ifndef SNMP3_NATIVE_CAPABILITIES_H
#define SNMP3_NATIVE_CAPABILITIES_H

#include "Binding.h"

namespace snmp3 {
Capabilities nativeCapabilities();
bool sameCapabilities(const Capabilities& expected, const Capabilities& observed);
}
#endif
