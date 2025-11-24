#pragma once

#include <cstddef>
#include <cstdint>
#include <stddef.h>
#include <sys/types.h>
#include <queue>
#include <vector>
#include <chrono>
#include <memory>

#include <types.hpp>

namespace ustacktcp {

class TCPEngine;
class RecvBuffer;

class SendBuffer {
    private:
        std::byte* buf_;
        static constexpr size_t sz_ = 65535; // FIXME: hardcoded max size
        size_t head_;
        size_t tail_;
        uint32_t next_seq_num_ = 100; //FIXME: hardcoded iss
        uint32_t ack_num_ = 100;
        
        std::priority_queue<std::shared_ptr<TCPSegment>, std::vector<std::shared_ptr<TCPSegment>>, TCPSegmentHeapCompare> in_flight_q_;
        std::priority_queue<std::shared_ptr<TCPSegment>, std::vector<std::shared_ptr<TCPSegment>>, TCPSegmentHeapCompare> next_q_;

        std::chrono::steady_clock::time_point to_expiry_;
        std::chrono::steady_clock::duration srtt_;
        std::chrono::steady_clock::duration rttvar_;
        std::chrono::steady_clock::duration rto_;
        bool rtt_init_;
        static constexpr std::chrono::steady_clock::duration INITIAL_RTO = std::chrono::milliseconds(200); 
        static constexpr std::chrono::steady_clock::duration RTO_MIN     = std::chrono::milliseconds(200);  
        static constexpr std::chrono::steady_clock::duration RTO_MAX     = std::chrono::seconds(60);
        static constexpr auto K           = 4;
        static constexpr std::chrono::steady_clock::duration G           = std::chrono::milliseconds(0);
        
        static constexpr auto TCP_RETRIES = 15;
        static constexpr auto TCP_SYN_RETRIES = 5;
        
        TCPEngine& engine_;
        RecvBuffer& recv_buf_;
        SocketAddr local_addr_;
        SocketAddr peer_addr_;
        
        static constexpr size_t MSS = 1460;
        static constexpr size_t INIT_SSTHRESH = 64*1024;
        static constexpr size_t INIT_CWND = 2*MSS;
        
        size_t in_flight_sz_ = 0;
        size_t rcvwnd_ = 0;
        size_t ssthresh_ = INIT_SSTHRESH;
        size_t cwnd_ = INIT_CWND;


        void updateCwnd(const size_t bytes_acked);
        bool canSend(const size_t n) const;
        
        bool isFull() const;
        bool isEmpty() const;
        size_t getSize() const;
        size_t getAvailSize() const;

        void rttSample(const std::chrono::steady_clock::duration);
        void restartRTO();

        void sendSegments();

    public:
        SendBuffer(TCPEngine&, RecvBuffer&);

        void setLocalAddr(const SocketAddr&);

        void setPeerAddr(const SocketAddr&);

        void setRcvWnd(const uint16_t rcvwnd);

        const uint32_t getSeqNumber() const;

        ssize_t enqueue(const std::byte* data, size_t len, const uint8_t flags);

        void handleACK(const uint32_t ack_num, const std::chrono::steady_clock::time_point ack_timestmp);

        void handleRTO();

        std::chrono::steady_clock::time_point getRTOExpiry() const;
};

}