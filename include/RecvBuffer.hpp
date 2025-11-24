#pragma once

#include <cstddef>
#include <cstdint>
#include <stddef.h>
#include <sys/types.h>
#include <map>
#include <memory>

#include <types.hpp>

namespace ustacktcp {

class RecvBuffer {
    private:
        std::byte* buf_;
        static constexpr size_t sz_ = 65535; // FIXME: hardcoded max size
        size_t tail_;
        size_t head_;
        uint32_t ack_;

        std::map<uint32_t, std::shared_ptr<TCPSegment>, TCPSegmentMapCompare> q_;

        size_t getSize() const;
        size_t getAvailSize() const;

    public:
        RecvBuffer();

        void setIRS(const uint32_t irs);

        ssize_t enqueue(const std::byte* data, const size_t len, const uint32_t seq_num, const uint8_t flags);

        ssize_t dequeue(std::byte* dest, const size_t len);

        uint32_t getAckNumber() const;

        bool availableData();

        uint16_t getWindowSize() const;
};

}