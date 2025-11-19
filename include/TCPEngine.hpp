#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include <types.hpp>
#include <TimerManager.hpp>

namespace ustacktcp {

struct EndpointHash {
    size_t operator()(const SocketAddr& e) const noexcept {
        return std::hash<uint64_t>()(((uint64_t)e.ip.addr << 16) | e.port);
    }
};
    
class RecvBuffer;
class StreamSocket;
    
class TCPEngine {
    private:
    // FIXME: create factory method for StreamSocket
    std::unordered_map<SocketAddr, std::shared_ptr<StreamSocket>, EndpointHash> bound;
    
    std::vector<std::shared_ptr<StreamSocket>> sockets_;
    
    int _raw_fd;
    
    friend std::shared_ptr<StreamSocket> make_socket(TCPEngine&);

    TimerManager timer_;

    public:
    
    TCPEngine();
    
    bool bind(const SocketAddr& addr, std::shared_ptr<StreamSocket> socket);
    
    ssize_t send(std::shared_ptr<TCPSegment>& seg, const SocketAddr& src_addr, const SocketAddr& dest_addr, const RecvBuffer& recv_buf);
    
    void recv();
};

std::shared_ptr<StreamSocket> make_socket(TCPEngine&);

}