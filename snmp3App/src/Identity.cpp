#include "Ipc.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <set>

namespace snmp3 { namespace ipc {
namespace {
const uint32_t Constants[64]={
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
uint32_t rotate(uint32_t v,unsigned n) { return (v>>n)|(v<<(32-n)); }
void block(std::array<uint32_t,8>& state,const uint8_t* data)
{
    uint32_t w[64];
    for(unsigned i=0;i<16;++i)w[i]=(uint32_t(data[4*i])<<24)|(uint32_t(data[4*i+1])<<16)|(uint32_t(data[4*i+2])<<8)|data[4*i+3];
    for(unsigned i=16;i<64;++i)w[i]=w[i-16]+(rotate(w[i-15],7)^rotate(w[i-15],18)^(w[i-15]>>3))+w[i-7]+(rotate(w[i-2],17)^rotate(w[i-2],19)^(w[i-2]>>10));
    auto a=state[0],b=state[1],c=state[2],d=state[3],e=state[4],f=state[5],g=state[6],h=state[7];
    for(unsigned i=0;i<64;++i) {
        const auto t1=h+(rotate(e,6)^rotate(e,11)^rotate(e,25))+((e&f)^(~e&g))+Constants[i]+w[i];
        const auto t2=(rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&b)^(a&c)^(b&c));
        h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    state[0]+=a;state[1]+=b;state[2]+=c;state[3]+=d;state[4]+=e;state[5]+=f;state[6]+=g;state[7]+=h;
}
}
std::array<uint8_t,32> digest(const std::string& file)
{
    std::ifstream stream(file,std::ios::binary); if(!stream)throw std::runtime_error("identity file unavailable");
    std::array<uint32_t,8> state={{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}};
    std::array<uint8_t,64> bytes{}; uint64_t total=0; size_t n=0;
    for(;;) {
        stream.read(reinterpret_cast<char*>(bytes.data()),bytes.size()); n=stream.gcount(); total=add(total,n);
        if(n!=bytes.size())break;
        block(state,bytes.data());
    }
    if(!stream.eof())throw std::runtime_error("identity read failed");
    bytes[n++]=128;
    if(n>56) { std::fill(bytes.begin()+n,bytes.end(),0); block(state,bytes.data()); bytes.fill(0); }
    else std::fill(bytes.begin()+n,bytes.end(),0);
    auto bits=multiply(total,8); for(unsigned i=0;i<8;++i)bytes[63-i]=uint8_t(bits>>(8*i)); block(state,bytes.data());
    std::array<uint8_t,32> result{};
    for(unsigned i=0;i<8;++i)for(unsigned j=0;j<4;++j)result[4*i+j]=uint8_t(state[i]>>(24-8*j));
    return result;
}
std::map<std::string,std::array<uint8_t,32>> loadedLibraries()
{
    std::ifstream stream("/proc/self/maps"); if(!stream)throw std::runtime_error("loader identity unavailable");
    std::set<std::string> paths; std::string line;
    while(std::getline(stream,line)) {
        auto start=line.find('/'); if(start==std::string::npos)continue; auto path=line.substr(start);
        if(path.find(".so")==std::string::npos)continue;
        if(path.size()>4096 || path.find(" (deleted)")!=std::string::npos)throw std::runtime_error("loader identity invalid");
        char* resolved=realpath(path.c_str(),nullptr); if(!resolved)throw std::runtime_error("loader identity invalid");
        paths.insert(resolved); free(resolved);
    }
    if(!stream.eof())throw std::runtime_error("loader identity read failed");
    std::map<std::string,std::array<uint8_t,32>> result;
    for(const auto& path:paths)result.emplace(path,digest(path));
    return result;
}
}}
