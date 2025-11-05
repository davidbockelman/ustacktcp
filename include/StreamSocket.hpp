#pragma once

#include <types.hpp>
#include <TCPEngine.hpp>
#include <SendBuffer.hpp>
#include <RecvBuffer.hpp>

namespace ustacktcp {

class TCPEngine;

class StreamSocket {
    private:
        TCPEngine& _engine;

        SendBuffer _send_buffer; // FIXME: randomize
        RecvBuffer _recv_buffer;

        uint32_t _iss = 1910533701; // FIXME: randomize

        SocketAddr _local_addr;
        SocketAddr _peer_addr;

        Frame createSYNFrame(const SocketAddr& dest_addr);

        Frame createSYNACKFrame(const SocketAddr& dest_addr, uint32_t ack_num);

        Frame createACKFrame(const SocketAddr& dest_addr, uint32_t ack_num);

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

        void handleSegment(const TCPSegment& segment, const SocketAddr& src_addr);
};

}