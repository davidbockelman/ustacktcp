#pragma once

#include <types.hpp>
#include <memory>
#include <unordered_map>
#include <vector>
#include <StreamSocket.hpp>

namespace ustacktcp {

class StreamSocket;

struct EndpointHash {
    size_t operator()(const SocketAddr& e) const noexcept {
        return std::hash<uint64_t>()(((uint64_t)e.ip.addr << 16) | e.port);
    }
};

class TCPEngine;

std::shared_ptr<StreamSocket> make_socket(TCPEngine&);

class TCPEngine {
    private:
        // FIXME: create factory method for StreamSocket
        std::unordered_map<SocketAddr, std::shared_ptr<StreamSocket>, EndpointHash> bound;

        std::vector<std::shared_ptr<StreamSocket>> sockets_;

        int _raw_fd;

        friend std::shared_ptr<StreamSocket> make_socket(TCPEngine&);
    public:

        TCPEngine();

        bool bind(const SocketAddr& addr, std::shared_ptr<StreamSocket> socket);

        ssize_t send(Frame& frame);

        void recv();
};

}