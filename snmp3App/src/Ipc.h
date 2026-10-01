#ifndef SNMP3_IPC_H
#define SNMP3_IPC_H

#include "Binding.h"
#include <array>
#include <limits>

namespace snmp3 { namespace ipc {
constexpr size_t HeaderBytes = 64;
constexpr uint32_t DataBytes = 2097152, BootstrapBytes = 16777216;
constexpr uint32_t ReadyBytes = 65536, RetiredBytes = 32768;
constexpr uint32_t MaxMembers = 1024, IoBytes = 65536, IoFrames = 64;
constexpr uint64_t DefaultCount = 1024, DefaultBytes = 1048576;
constexpr uint64_t MaxCount = 16384, MaxBytes = 67108864;
enum class Kind : uint16_t { Bootstrap=1, Ready=2, Batch=3, Result=4,
                             Retired=5, Close=6, Closed=7, Fault=8 };
enum class Outcome : uint16_t { Complete=1, Deadline=2, ChannelFailure=3,
                                WorkerFailure=4, Stopping=5, NativeFailure=6 };
struct Header {
    Kind kind = Kind::Fault;
    uint32_t length = 0;
    uint64_t activation = 0, epoch = 0, address = 0, revision = 0, batch = 0, sequence = 0;
};
struct Identity {
    uint64_t binding = 0, generation = 0, admission = 0;
    bool operator==(const Identity& other) const;
};
struct Command {
    uint64_t definition = 0;
    Identity id;
    uint64_t deadline = 0;
    Operation operation = Operation::Get;
    std::vector<uint8_t> value;
};
struct Result {
    Identity id;
    Outcome outcome = Outcome::NativeFailure;
    uint16_t nativeOutcome = 0;
    int64_t errorStatus = 0, errorIndex = 0;
    std::vector<uint8_t> value;
};
uint64_t add(uint64_t a, uint64_t b);
uint64_t multiply(uint64_t a, uint64_t b);
size_t size(uint64_t value);
uint32_t ceiling(Kind kind);
uint16_t tag(ValueType type);
ValueType type(uint16_t tag);
struct Ready {
    Capabilities capabilities;
    std::array<uint8_t,32> executable{};
    std::map<std::string,std::array<uint8_t,32>> libraries;
};
std::array<uint8_t,32> digest(const std::string& file);
std::map<std::string,std::array<uint8_t,32>> loadedLibraries();
std::vector<uint8_t> encodeReady(const Ready& ready);
Ready decodeReady(const std::vector<uint8_t>& bytes);
bool sameCapabilities(const Capabilities& a, const Capabilities& b);
uint64_t responseBytes(const BindingDefinition& binding);
uint64_t charge(const BindingDefinition& binding);
std::string addressKey(const std::string& address);

class Writer {
public:
    explicit Writer(size_t limit, size_t reservation=0, bool measure=false) : maximum(limit), measuring(measure)
    {
        if(reservation>maximum)throw std::runtime_error("IPC allocation exceeds ceiling");
        if(reservation) { data.reserve(reservation); if(data.capacity()!=reservation)throw std::runtime_error("IPC capacity exceeds inventory"); }
    }
    void u16(uint16_t value);
    void u32(uint32_t value);
    void u64(uint64_t value);
    void raw(const void* value, size_t length);
    void bytes(const std::vector<uint8_t>& value);
    void text(const std::string& value);
    void oid(const std::vector<uint32_t>& value);
    std::vector<uint8_t> finish() { return std::move(data); }
    size_t length() const { return position; }
private:
    size_t maximum;
    size_t position=0;
    bool measuring;
    std::vector<uint8_t> data;
};
class Reader {
public:
    Reader(const uint8_t* value, size_t length, uint64_t storageLimit=UINT64_MAX)
        : data(value), remaining(length), ownedLimit(storageLimit) {}
    explicit Reader(const std::vector<uint8_t>& value) : Reader(value.data(), value.size()) {}
    uint16_t u16();
    uint32_t u32();
    uint64_t u64();
    const uint8_t* raw(size_t length);
    std::vector<uint8_t> bytes(size_t maximum);
    std::string text(size_t maximum);
    std::vector<uint32_t> oid();
    size_t left() const { return remaining; }
    void end() const;
    void reserveOwned(uint64_t bytes);
private:
    const uint8_t* data;
    size_t remaining;
    uint64_t ownedLimit, owned=0;
};
std::array<uint8_t, HeaderBytes> encodeHeader(const Header& value);
Header decodeHeader(const std::array<uint8_t, HeaderBytes>& value);
std::vector<uint8_t> encodeValue(const Value& value);
Value decodeValue(Reader& reader, const BindingDefinition& binding, bool set);
std::vector<uint8_t> encodeCommands(const std::vector<Command>& values);
std::vector<Command> decodeCommands(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> encodeResults(const std::vector<Result>& values);
std::vector<Result> decodeResults(const std::vector<uint8_t>& bytes);
struct ResultReservation {
    Identity id;
    const BindingDefinition* definition;
};
class StaleIdentity : public std::runtime_error {
public:
    StaleIdentity() : std::runtime_error("stale IPC identity") {}
};
std::vector<Result> decodeResults(const std::vector<uint8_t>& bytes,
                                  const std::vector<ResultReservation>& reservations);
std::vector<uint8_t> encodeRetired(const std::vector<Identity>& values);
std::vector<Identity> decodeRetired(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> encodeBootstrap(const Configuration& config, const std::string& address = std::string());
size_t bootstrapBytes(const Configuration& config,const std::string& address);
uint64_t bootstrapOwnedBytes(const Configuration& config,const std::string& address);
std::shared_ptr<const Configuration> decodeBootstrap(const std::vector<uint8_t>& bytes,
                                                   std::map<uint64_t,std::string>* definitions = nullptr);

// Partial storage is allocated only after the owner validates its identity and reservation.
class Channel {
public:
    explicit Channel(int fd);
    ~Channel();
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;
    int descriptor() const { return fd; }
    bool writing() const { return txHeaderOffset < HeaderBytes || txOffset < tx.size(); }
    bool partial() const { return rxHeaderOffset != 0; }
    uint64_t partialOrigin() const { return rxOrigin; }
    uint64_t bytesSent() const { return sent; }
    void queue(Header header, std::vector<uint8_t> body);
    bool write(size_t& budget);
    bool readHeader(size_t& budget, uint64_t now, Header& header);
    void acceptBody(bool retain);
    bool readBody(size_t& budget, std::vector<uint8_t>& body);
    size_t pull(size_t& budget, uint8_t* target, size_t capacity);
    void finishBody();
    void close();
private:
    int fd;
    std::array<uint8_t, HeaderBytes> txHeader{}, rxHeader{};
    std::array<uint8_t, IoBytes> discard{};
    std::vector<uint8_t> tx, rx;
    size_t txHeaderOffset = HeaderBytes, txOffset = 0, rxHeaderOffset = 0, rxOffset = 0;
    uint32_t rxLength = 0;
    uint64_t rxOrigin = 0, sent = 0;
    bool bodyAccepted = false, retaining = false;
};

// Member prefixes establish immutable bounds before any SET storage is allocated.
class BatchReader {
public:
    using Definitions = std::map<uint64_t,std::shared_ptr<const Binding>>;
    explicit BatchReader(const Definitions& definitions) : definitions(definitions) {}
    void begin(Channel& channel, uint32_t length, uint64_t admissionFloor=0);
    bool read(Channel& channel, size_t& budget, std::vector<Command>& commands);
    void discard();
private:
    const Definitions& definitions;
    std::array<uint8_t,48> prefix{};
    std::vector<Command> pending;
    uint32_t remaining=0, count=0, valueLength=0;
    uint64_t admissionFloor=0;
    size_t prefixUsed=0, valueUsed=0;
    bool started=false, haveCount=false, haveMember=false;
};
}}
#endif
