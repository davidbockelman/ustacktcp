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
    public:
        SocketState _state = SocketState::CLOSED;
        StreamSocket() = default;


        bool bind(const SocketAddr& addr)
        {
            return _engine.bind(addr, this);
        }

        bool connect(const SocketAddr& addr);

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
