#include <iostream>
#include <unordered_map>
#include <memory>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip.h>     // struct iphdr
#include <arpa/inet.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <net/if.h>

class StreamSocket;

struct IPAddr {
    uint32_t addr;     // IPv4 in network byte order

    IPAddr(uint32_t a) : addr(a) {}

    bool operator==(const IPAddr& other) const {
        return addr == other.addr;
    }
};

struct SocketAddr {
    IPAddr ip;
    uint16_t port;     // in network byte order

    SocketAddr(IPAddr i, uint16_t p) : ip(i), port(p) {}

    bool operator==(const SocketAddr& other) const {
        return (ip == other.ip) && (port == other.port);
    }
};

struct EndpointHash {
    size_t operator()(const SocketAddr& e) const noexcept {
        return std::hash<uint64_t>()(((uint64_t)e.ip.addr << 16) | e.port);
    }
};

class TCPEngine {
    private:
        std::unordered_map<SocketAddr, StreamSocket*, EndpointHash> bound;
        std::unordered_map<SocketAddr, StreamSocket*, EndpointHash> listeners;
        std::unordered_map<SocketAddr, StreamSocket*, EndpointHash> connections;

        int _raw_fd;
    public:

        TCPEngine() 
        {
            _raw_fd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
            if (_raw_fd < 0)
            {
                perror("TCPEngine::socket");
                exit(1);
            }
        }

        bool bind(const SocketAddr& addr, StreamSocket* socket) {
            if (bound.find(addr) != bound.end()) {
                return false;
            }
            bound[addr] = socket;
            return true;
        }

        ssize_t send(const StreamSocket* sock, const void* buf, size_t len)
        {
            // if (sock->_state != SocketState::ESTABLISHED) return -1;
            // // if (connections.find(sock) == connections.end()) return -1;
            // unsigned char frame[1500];
            // memcpy(frame, buf, len);

            // sockaddr_in dst{};
            // dst.sin_family = AF_INET;
            // dst.sin_addr.s_addr = inet_addr("192.168.86.1");

            // return sendto(_raw_fd,
            //           frame, len,
            //           0,
            //           (sockaddr*)&dst, sizeof(dst));
        }
};

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

    TCPOptions options;

    ssize_t write(unsigned char* buf) const
    {
        memcpy(buf, &src_port, sizeof(src_port));
        memcpy(buf + 2, &dst_port, sizeof(dst_port));
        memcpy(buf + 4, &seq_num, sizeof(seq_num));
        memcpy(buf + 8, &ack_num, sizeof(ack_num));
        memcpy(buf + 12, &data_offset, sizeof(data_offset));
        memcpy(buf + 13, &flags, sizeof(flags));
        memcpy(buf + 14, &window_size, sizeof(window_size));
        memcpy(buf + 16, &checksum, sizeof(checksum));
        memcpy(buf + 18, &urgent_pointer, sizeof(urgent_pointer));
        return 20; // size of TCP header without options
    }
};

struct TCPOptions {
    // To be implemented
};

struct PseudoIPv4Header {
    uint32_t src_addr;
    uint32_t dst_addr;
    uint8_t zero;
    uint8_t protocol;
    uint16_t tcp_length;

    PseudoIPv4Header(uint32_t src, uint32_t dst, uint16_t len) :
        src_addr(src), dst_addr(dst), zero(0), protocol(IPPROTO_TCP), tcp_length(htons(len)) {}

    ssize_t write(unsigned char* buf) const
    {
        memcpy(buf, &src_addr, sizeof(src_addr));
        memcpy(buf + 4, &dst_addr, sizeof(dst_addr));
        memcpy(buf + 8, &zero, sizeof(zero));
        memcpy(buf + 9, &protocol, sizeof(protocol));
        memcpy(buf + 10, &tcp_length, sizeof(tcp_length));
        return 12;
    }
};

struct TCPData {
    const unsigned char* payload;
    size_t payload_len;

    TCPData(const unsigned char* p, size_t len) : payload(p), payload_len(len) {}
};

struct ChecksumBuilder {
    private:
        uint32_t _sum = 0;

    public:
        void add(const void* buf, size_t len)
        {
            const uint16_t* words = static_cast<const uint16_t*>(buf);
            for (size_t i = 0; i < len / 2; ++i)
            {
                _sum += ntohs(words[i]);
            }
            if (len % 2)
            {
                _sum += ntohs(static_cast<const uint8_t*>(buf)[len - 1] << 8);
            }
        }

        uint16_t finalize()
        {
            while (_sum >> 16)
            {
                _sum = (_sum & 0xFFFF) + (_sum >> 16);
            }
            return htons(static_cast<uint16_t>(~_sum));
        }
};

struct Frame {
    private:
        unsigned char* _buf;
        size_t _cap;
        size_t _len;

    public:
        Frame(PseudoIPv4Header& iphdr, TCPHeader& tcphdr, TCPData& payload) {

        }

        bool append(const void* data, size_t len)
        {
            if (_len + len > _cap) return false;
            memcpy(_buf + _len, data, len);
            _len += len;
            return true;
        }

        size_t length() const { return _len; }

        const unsigned char* data() const { return _buf; }
};


void write_tcp_header(unsigned char* buf, const TCPHeader& header, const TCPOptions& options, const unsigned char* payload, size_t payload_len)
{
    size_t opt_len = 0; //FIXME: implement options length

    uint8_t hdr_len = sizeof(TCPHeader) + opt_len;
    uint16_t total_len = hdr_len + payload_len;
    hdr_len = (hdr_len + 3) / 4; // in 32-bit words
    hdr_len <<= 4;

    PseudoIPv4Header pseudo_header(0, 0, total_len); // FIXME: set real IPs
    pseudo_header.write(buf); // FIXME: handle errors
    
    memcpy(buf, &header.src_port, sizeof(header.src_port));
    memcpy(buf + 2, &header.dst_port, sizeof(header.dst_port));
    memcpy(buf + 4, &header.seq_num, sizeof(header.seq_num));
    memcpy(buf + 8, &header.ack_num, sizeof(header.ack_num));
    memcpy(buf + 12, &hdr_len, sizeof(hdr_len));
    memcpy(buf + 13, &header.flags, sizeof(header.flags));
    memcpy(buf + 14, &header.window_size, sizeof(header.window_size));
    memset(buf + 16, 0, sizeof(header.checksum)); // checksum set to 0 for calculation
    memcpy(buf + 18, &header.urgent_pointer, sizeof(header.urgent_pointer));
    // TODO: write options
    memcpy(buf + hdr_len, payload, payload_len);

    ChecksumBuilder checksum_builder;
    

}

class StreamSocket {
    private:
        TCPEngine& _engine;
        
        bool _send_closed;
        
        size_t _send_buf_cap = 64 * 1024; // FIXME: make constant
        size_t _send_buf_sz = 0;

        // TODO: encapsulate in peer info obj
        uint32_t _snd_wnd;
        uint32_t _snd_una;

        uint32_t _cwnd;

        uint32_t _iss = 0; // FIXME: randomize

    public:
        SocketState _state = SocketState::CLOSED;
        StreamSocket() = default;


        bool bind(const SocketAddr& addr)
        {
            return _engine.bind(addr, this);
        }

        bool connect(const SocketAddr& addr)
        {
            // Send SYN

            // Wait for SYN or SYN-ACK
            // SYN -> SYN_RECEIVED
            // SYN-ACK -> ESTABLISHED
        }

        bool listen();

        StreamSocket& accept();

        bool close();

        bool shutdown();

        ssize_t send(const void* buf, size_t len)
        {
            if (_state != SocketState::ESTABLISHED && _state != SocketState::CLOSE_WAIT)
            {
                return -1; // FIXME: add error code
            }

            if (_send_closed)
            {
                return -1; // FIXME: add error code
            }

            if (_send_buf_cap <= _send_buf_sz + len)
            {
                return -1; // FIXME: add error code
            }


        }

        ssize_t recv(void* buf, size_t len);
};



int main() {
    return 0;
}
