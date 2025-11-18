#include <cstddef>
#include <cstdint>
#include <stddef.h>
#include <sys/types.h>
#include <queue>
#include <vector>
#include <chrono>
#include <TCPEngine.hpp>
#include <types.hpp>

namespace ustacktcp {


class SendBuffer {
    private:
        std::byte* buf_;
        static constexpr size_t sz_ = 65535; // FIXME: hardcoded max size
        size_t head_;
        size_t tail_;
        uint32_t next_seq_num_ = 100; //FIXME: hardcoded iss
        uint32_t ack_num_ = 100;

        std::priority_queue<std::shared_ptr<TCPSegment>, std::vector<std::shared_ptr<TCPSegment>>, TCPSegmentCompare> in_flight_q_;
        std::priority_queue<std::shared_ptr<TCPSegment>, std::vector<std::shared_ptr<TCPSegment>>, TCPSegmentCompare> next_q_;

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

        TCPEngine& engine_;
        RecvBuffer& recv_buf_;
        SocketAddr local_addr_;
        SocketAddr peer_addr_;
        
        
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

        ssize_t enqueue(const std::byte* data, size_t len, const uint8_t flags);

        void handleACK(const uint32_t ack_num, const std::chrono::steady_clock::time_point ack_timestmp);

        void handleRTO();
};

}