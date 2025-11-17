#include <cstddef>
#include <cstdint>
#include <stddef.h>
#include <sys/types.h>
#include <queue>
#include <vector>
#include <chrono>

namespace ustacktcp {

struct SentSegment {
    std::byte* const data_;
    uint32_t seq_start_;
    uint32_t seq_end_;
    size_t retransmit_cnt_;
    std::chrono::steady_clock::time_point send_tmstp_;

    bool operator<(const SentSegment& o) const
    {
        return seq_start_ > o.seq_start_;
    }
};


class SendBuffer {
    private:
        std::byte* buf_;
        static constexpr size_t sz_ = 65535; // FIXME: hardcoded max size
        size_t una_;
        size_t nxt_;
        size_t wnd_;
        uint32_t iss_;
        size_t head_;

        std::priority_queue<SentSegment, std::vector<SentSegment>, std::greater<SentSegment>> q_;

        std::chrono::steady_clock::time_point to_expiry_;
        std::chrono::steady_clock::duration rtt_;

        uint32_t getBytesInFlight() const;

    public:
        SendBuffer();

        void build(uint32_t iss);

        ssize_t enqueue(const std::byte* data, size_t len);

        std::byte* getData(size_t& len);

        uint32_t getSeqNumber() const;

        void handleRTO();

        void ack(uint32_t ack_num);

        void setWindowSize(uint16_t wnd);
};

}