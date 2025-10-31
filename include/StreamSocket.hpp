#pragma once

#include <types.hpp>
#include <TCPEngine.hpp>

namespace ustacktcp {

class TCPEngine;

class StreamSocket {
    private:
        TCPEngine& _engine;

        uint32_t _iss = 0; // FIXME: randomize

        SocketAddr _local_addr;
        SocketAddr _peer_addr;

        Frame createSYNFrame(const SocketAddr& dest_addr);

    public:
        SocketState _state = SocketState::CLOSED;
        // FIXME: delete this constructor and use factory method
        StreamSocket(TCPEngine& engine);


        bool bind(const SocketAddr& addr);

        bool connect(const SocketAddr& addr);

        bool listen();

        StreamSocket& accept();

        bool close();

        bool shutdown();

        ssize_t send(const void* buf, size_t len);

        ssize_t recv(void* buf, size_t len);
};

}