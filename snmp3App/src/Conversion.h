#ifndef SNMP3_CONVERSION_H
#define SNMP3_CONVERSION_H

#include "Binding.h"
#include <string>

namespace snmp3 {
enum class ScalarType { Signed32, Unsigned32, Signed64, Unsigned64, Float32, Float64 };
bool integerType(ValueType type);
bool numericType(ValueType type);
bool textualType(ValueType type);
float roundBinary32(double value);
Value convertNumeric(const Value& value, ValueType target, bool permitRounding = false);
Value convertScalar(const Value& value, ScalarType target);
std::string convertText(const Value& value, size_t dataCapacity);
Value captureText(const char* buffer, size_t storageBytes, size_t bindingCapacity,
                  size_t lengthIncludingNul = 0);
}
#endif
