
#pragma once


#include <mutex>
#include <condition_variable>
#include <map>
#include <chrono>
#include <optional>

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
        SendBuffer _send_buffer; 

        SocketAddr _local_addr;
        SocketAddr _peer_addr;

        std::mutex m_;
        std::condition_variable cv_;

        std::chrono::steady_clock::time_point time_wait_expiry_;
        static constexpr std::chrono::steady_clock::duration time_wait_to_ = std::chrono::seconds(60);

        void setTimeWaitExipiry();
        void timeWaitTO();

        bool validSeqNum(uint32_t seq_start, size_t len) const;
        bool validFlags(uint8_t flags) const;

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

        bool close();

        ssize_t send(const std::byte* buf, size_t len);

        ssize_t recv(std::byte* buf, size_t len);

        std::optional<uint8_t> handleCntrl(const TCPHeader& tcphdr, const SocketAddr& src_addr, const size_t data_len);
};

}