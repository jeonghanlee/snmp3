#ifndef SNMP3_CONFIG_H
#define SNMP3_CONFIG_H

#include "Binding.h"
#include <epicsMutex.h>

namespace snmp3 {
struct ConfigState {
    std::shared_ptr<const Configuration> configuration;
    uint64_t revision;
    bool frozen;
    std::string workerPath;
};
class Config {
public:
    Config();
    static Config& instance();
    void load(const std::string& file, const std::string& nativeProbe);
    std::shared_ptr<const Binding> bind(const std::string& id);
    void freeze();
    void setWorkerPath(const std::string& absolutePath);
    ConfigState snapshot();
    void report();
private:
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    epicsMutex mutex;
    std::shared_ptr<const Configuration> current;
    uint64_t revision = 0;
    bool frozen = false;
    std::string workerPath;
};
}
#endif
