#include <cstddef>
#include <cstdint>
#include <stddef.h>
#include <sys/types.h>

class SendBuffer {
    private:
        std::byte* buf_;
        static constexpr size_t sz_ = 65535; // FIXME: hardcoded max size
        size_t una_;
        size_t nxt_;
        size_t wnd_;
        uint32_t iss_;
        size_t head_;

    public:
        SendBuffer();

        void build(uint32_t iss);

        ssize_t enqueue(std::byte* data, size_t len);

        uint32_t getSeqNumber() const;

        void ack(uint32_t ack_num);

        void setWindowSize(uint16_t wnd);
};