#ifndef SNMP3_VALUE_H
#define SNMP3_VALUE_H

#include <array>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace snmp3 {
enum class ValueType {
    Integer, Unsigned32, Counter32, Gauge32, TimeTicks, Counter64,
    Octets, ObjectId, IpAddress, OpaqueFloat, OpaqueDouble,
    NoSuchObject, NoSuchInstance, EndOfMibView
};

class Value {
public:
    static Value integer(int64_t value) { Value v(ValueType::Integer); v.signedData = value; return v; }
    static Value unsigned32(ValueType type, uint32_t value)
    {
        if (type != ValueType::Unsigned32 && type != ValueType::Counter32 &&
            type != ValueType::Gauge32 && type != ValueType::TimeTicks)
            throw std::logic_error("invalid unsigned value tag");
        Value v(type); v.unsignedData = value; return v;
    }
    static Value counter64(uint64_t value) { Value v(ValueType::Counter64); v.unsignedData = value; return v; }
    static Value octets(std::vector<uint8_t> value) { Value v(ValueType::Octets); v.bytes = std::move(value); return v; }
    static Value objectId(std::vector<uint32_t> value) { Value v(ValueType::ObjectId); v.arcs = std::move(value); return v; }
    static Value ipAddress(std::array<uint8_t, 4> value) { Value v(ValueType::IpAddress); v.ip = value; return v; }
    static Value opaqueFloat(float value) { Value v(ValueType::OpaqueFloat); v.floatData = value; return v; }
    static Value opaqueDouble(double value) { Value v(ValueType::OpaqueDouble); v.doubleData = value; return v; }
    static Value exception(ValueType type)
    {
        if (type != ValueType::NoSuchObject && type != ValueType::NoSuchInstance &&
            type != ValueType::EndOfMibView) throw std::logic_error("invalid exception tag");
        return Value(type);
    }
    ValueType type() const { return tag; }
    int64_t integer() const { require(ValueType::Integer); return signedData; }
    uint32_t unsigned32() const
    {
        if (tag != ValueType::Unsigned32 && tag != ValueType::Counter32 &&
            tag != ValueType::Gauge32 && tag != ValueType::TimeTicks)
            throw std::logic_error("value tag mismatch");
        return static_cast<uint32_t>(unsignedData);
    }
    uint64_t counter64() const { require(ValueType::Counter64); return unsignedData; }
    const std::vector<uint8_t>& octets() const { require(ValueType::Octets); return bytes; }
    const std::vector<uint32_t>& objectId() const { require(ValueType::ObjectId); return arcs; }
    std::array<uint8_t, 4> ipAddress() const { require(ValueType::IpAddress); return ip; }
    float opaqueFloat() const { require(ValueType::OpaqueFloat); return floatData; }
    double opaqueDouble() const { require(ValueType::OpaqueDouble); return doubleData; }
private:
    explicit Value(ValueType type) : tag(type) {}
    void require(ValueType expected) const
    {
        if (tag != expected) throw std::logic_error("value tag mismatch");
    }
    ValueType tag;
    int64_t signedData = 0;
    uint64_t unsignedData = 0;
    float floatData = 0;
    double doubleData = 0;
    std::vector<uint8_t> bytes;
    std::vector<uint32_t> arcs;
    std::array<uint8_t, 4> ip = {{0, 0, 0, 0}};
};
}
#endif
