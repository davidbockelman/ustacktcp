#pragma once

#include <types.hpp>
#include <memory>
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
        // FIXME: create factory method for StreamSocket
        std::unordered_map<SocketAddr, StreamSocket*, EndpointHash> bound;

        int _raw_fd;
    public:

        TCPEngine();

        bool bind(const SocketAddr& addr, StreamSocket* socket);

        ssize_t send(StreamSocket* sock, Frame& frame);

        void recv();
};

}