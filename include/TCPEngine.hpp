#pragma once

#include <types.hpp>
#include <unordered_map>
#include <StreamSocket.hpp>

namespace ustacktcp {

class StreamSocket;

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

        TCPEngine();

        bool bind(const SocketAddr& addr, StreamSocket* socket);

        ssize_t send(const StreamSocket* sock, const Frame& frame);
};

}