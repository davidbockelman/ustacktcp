#pragma once

#include <types.hpp>
#include <TCPEngine.hpp>
#include <SendBuffer.hpp>
#include <RecvBuffer.hpp>
#include <mutex>
#include <condition_variable>
#include <map>
#include <chrono>

namespace ustacktcp {

class TCPEngine;

class StreamSocket : public std::enable_shared_from_this<StreamSocket> {
    private:
        TCPEngine& _engine;

        RecvBuffer _recv_buffer;
        SendBuffer _send_buffer; // FIXME: randomize

        uint32_t _iss = 1910533701; // FIXME: randomize

        SocketAddr _local_addr;
        SocketAddr _peer_addr;

        std::mutex m_;
        std::condition_variable cv_;

        friend std::shared_ptr<StreamSocket> make_socket(TCPEngine&);
    public:
        StreamSocket(TCPEngine& engine);
        SocketState _state = SocketState::CLOSED;
        // FIXME: delete this constructor and use factory method
        StreamSocket(const StreamSocket&) = delete;
        StreamSocket& operator=(const StreamSocket&) = delete;

        bool bind(const SocketAddr& addr);

        bool connect(const SocketAddr& addr);

        bool listen();

        StreamSocket& accept();

        bool close();

        bool shutdown();

        ssize_t send(const std::byte* buf, size_t len);

        ssize_t recv(std::byte* buf, size_t len);

        void handleSegment(const TCPSegment& segment, const SocketAddr& src_addr);
};

}