#include "Scheduler.h"
#include "NativeInternal.h"
#include <cstdio>

using namespace snmp3;
int main()
{
    using RequestNode=std::_Rb_tree_node<std::pair<const uint64_t,std::unique_ptr<detail::Request>>>;
    const auto base=Scheduler::fixedGenerationBytes();
    const uint64_t native=sizeof(detail::Request)+sizeof(RequestNode)+sizeof(NativeVariable)+
                          sizeof(std::pair<Native::Completion,NativeResult>)+128*sizeof(oid);
    std::printf("{\"scheduler_ipc_fixed\":%llu,\"native_fixed\":%llu,\"aggregate_fixed\":%llu,\"ceiling\":4096,"
                "\"native_session_object\":%zu,\"security_material_object\":%zu,\"oid_width\":%zu,\"event_slot\":%zu}\n",
                (unsigned long long)base,(unsigned long long)native,(unsigned long long)(base+native),
                sizeof(detail::Session),sizeof(detail::SecurityMaterial),sizeof(oid),sizeof(NativeEvent)+65);
    return base+native<=4096 && sizeof(NativeEvent)+65<=256 ? 0:1;
}
