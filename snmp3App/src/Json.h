#ifndef SNMP3_JSON_H
#define SNMP3_JSON_H

#include <map>
#include <string>
#include <vector>

namespace snmp3 {
struct Json {
    enum class Kind { Null, Boolean, Number, String, Object, Array };
    Kind kind = Kind::Null;
    bool boolean = false;
    std::string text;
    std::map<std::string, Json> object;
    std::vector<Json> array;
    static Json parse(const std::string& input);
};
}
#endif
