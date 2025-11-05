#include <cstddef>
#include <cstdint>
#include <stddef.h>
#include <sys/types.h>

class RecvBuffer {
    private:
        std::byte* buf_;
        static constexpr size_t sz_ = 65535; // FIXME: hardcoded max size
        size_t tail_;
        size_t nxt_;
        uint32_t irs_;

    public:
        RecvBuffer();

        void build(uint32_t irs);

        ssize_t enqueue(uint32_t seq_num, const std::byte* data, size_t len);

        void push(std::byte* dest, size_t& len);

        uint32_t getAckNumber() const;

        uint16_t availableDataSize() const;

        uint16_t getWindowSize() const;
};