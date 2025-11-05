#pragma once

#include <cstdint>
#include <cstring>
#include <unistd.h>
#include <cstddef>

namespace ustacktcp {


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

struct TCPData {
    const std::byte* payload;
    size_t payload_len;

    TCPData() = default;
    
    TCPData(const TCPData& other) = default;
    
    TCPData(const std::byte* p, size_t len);

    int writeNetworkBytes(std::byte* buf) const;
};

struct InternetChecksumBuilder {
    private:
        uint32_t _sum = 0;

    public:
        void add(const void* buf, size_t len);

        uint16_t finalize();
};

struct Frame {
    private:
        PseudoIPv4Header _iphdr;
        TCPHeader _tcphdr;
        TCPData _payload;
    
        std::byte* _buf;
        size_t _cap;
        size_t _len;

    public:
        Frame();
    
        Frame(PseudoIPv4Header& iphdr, TCPHeader& tcphdr, TCPData& payload);

        int writeNetworkBytes();

        int computeAndWriteChecksum();

        size_t getTCPSegmentLength() const;

        const std::byte* getTCPSegmentBuffer() const;

        const uint32_t getDestinationIP() const;

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
};

struct TCPSegment {
    TCPHeader header;
    TCPOptions options;
    TCPData data;

    TCPSegment(TCPHeader h, TCPOptions o, TCPData d);
};

// FIXME: what size int is this?
enum TCPFlag {
    FIN = 0x01,
    SYN = 0x02,
    RST = 0x04,
    PSH = 0x08,
    ACK = 0x10,
    URG = 0x20
};

}