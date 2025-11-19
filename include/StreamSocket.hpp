
#pragma once


#include <mutex>
#include <condition_variable>
#include <map>
#include <chrono>

#include <types.hpp>
#include <SendBuffer.hpp>
#include <RecvBuffer.hpp>
#include <TCPEngine.hpp>
#include <TimerManager.hpp>

namespace ustacktcp {

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
        friend void TCPEngine::recv();
        friend void TimerManager::timeoutLoop();
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

        void handleCntrl(const TCPHeader& tcphdr, const SocketAddr& src_addr);
};

}