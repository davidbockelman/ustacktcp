#pragma once

#include <cstdint>
#include <cstring>
#include <unistd.h>
#include <cstddef>
#include <chrono>
#include <memory>

namespace ustacktcp {

#define SEQ_LT(a,b)   ((int32_t)((a) - (b)) < 0)
#define SEQ_LEQ(a,b)  ((int32_t)((a) - (b)) <= 0)
#define SEQ_GT(a,b)   ((int32_t)((a) - (b)) > 0)
#define SEQ_GEQ(a,b)  ((int32_t)((a) - (b)) >= 0)


enum SocketState {
    CLOSED,
    LISTEN,
    SYN_SENT,
    SYN_RECEIVED,
    ESTABLISHED,
    FIN_WAIT_1,
    FIN_WAIT_2,
    CLOSE_WAIT,
    LAST_ACK,
    TIME_WAIT,
    CLOSING
};

struct TCPHeader {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t data_offset; // 4 bits
    uint8_t flags;       // 6 bits
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_pointer;

    TCPHeader() = default;

    TCPHeader(const std::byte* buf);

    void switchByteOrder();
    
    static size_t getCheckSumOffset();

    int writeNetworkBytes(std::byte* buf) const;
};

struct IPAddr {
    uint32_t addr;     // IPv4 in network byte order

    IPAddr(uint32_t a);

    bool operator==(const IPAddr& other) const;
};

struct SocketAddr {
    IPAddr ip;
    uint16_t port;     // in network byte order

    SocketAddr(IPAddr i, uint16_t p);
    SocketAddr();

    bool operator==(const SocketAddr& other) const;
};


struct TCPOptions {
    // TODO: To be implemented
};

struct PseudoIPv4Header {
    uint32_t src_addr;
    uint32_t dst_addr;
    uint8_t zero;
    uint8_t protocol;
    uint16_t tcp_length;

    PseudoIPv4Header();
    
    PseudoIPv4Header(uint32_t src, uint32_t dst, uint16_t tcp_len);

    int writeNetworkBytes(std::byte* buf) const;
};

struct InternetChecksumBuilder {
    private:
        uint32_t _sum = 0;

    public:
    InternetChecksumBuilder();

        void add(const void* buf, size_t len);

        uint16_t finalize();
};


// TODO: include options
struct IPHeader {
    uint8_t version_ihl; // Version (4 bits) + Internet header length (4 bits)
    uint8_t tos;         // Type of service
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_fragment_offset;
    uint8_t ttl;        // Time to live
    uint8_t protocol;
    uint16_t header_checksum;
    uint32_t src_addr;
    uint32_t dst_addr;

    IPHeader() = default;

    IPHeader(const std::byte* buf);

    bool nextProtoIsTCP() const;

    size_t getHeaderLength() const;

    uint8_t getVersion() const;
};

struct TCPSegment {
    const std::byte* data_;
    const std::byte* data2_;
    uint32_t seq_start_;
    uint32_t len_;
    uint32_t brk_len_;
    size_t retransmit_cnt_;
    uint8_t flags_;
    std::chrono::steady_clock::time_point send_tmstp_;

    TCPSegment(const std::byte* data, const std::byte* data2, uint32_t seq_start, uint32_t len, uint32_t brk_len, uint8_t flags)
    :   data_(data),
        data2_(data2),
        seq_start_(seq_start),
        len_(len),
        brk_len_(brk_len),
        retransmit_cnt_(0),
        flags_(flags),
        send_tmstp_(std::chrono::steady_clock::now())
    {}
};

struct TCPSegmentHeapCompare {
    bool operator()(const std::shared_ptr<TCPSegment>& a, const std::shared_ptr<TCPSegment>& b) const
    {
        return a->seq_start_ > b->seq_start_;
    }
};

struct TCPSegmentMapCompare {
    bool operator()(uint32_t a, uint32_t b) const {
        // true if a is "less than" b in TCP seq space
        return SEQ_LT(a, b);
    }
};

enum TCPFlag : uint8_t {
    FIN = 0x01,
    SYN = 0x02,
    RST = 0x04,
    PSH = 0x08,
    ACK = 0x10,
    URG = 0x20
};

}