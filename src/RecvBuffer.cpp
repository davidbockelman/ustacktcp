#include <RecvBuffer.hpp>
#include <cstring>
#include <algorithm>
#include <iostream>

RecvBuffer::RecvBuffer() {}

void RecvBuffer::build(uint32_t irs)
{
    irs_ = irs;
    tail_ = irs;
    nxt_ = irs + 1;
    buf_ = new std::byte[sz_]; // FIXME: hardcoded max size
}

ssize_t RecvBuffer::enqueue(uint32_t seq_num, const std::byte* data, size_t len)
{
    if (seq_num < nxt_) // already received
        return 0;
    if (seq_num > nxt_) // out of order
        return -1; // FIXME: add error code
    auto bLen = (nxt_ + sz_ - tail_) % sz_;
    auto free_sz = sz_ - bLen;
    if (free_sz < len) return -1; // FIXME: add error code
    auto firstBlockSz = std::min(sz_-(nxt_%sz_),len);
    auto firstBlockStart = buf_+(nxt_%sz_);
    memcpy(firstBlockStart, data, firstBlockSz);
    if (firstBlockSz < len)
    {
        auto secondBlockSz = len - firstBlockSz;
        memcpy(buf_,data+firstBlockSz,secondBlockSz);
    }
    nxt_ += len;
    return len;
}

ssize_t RecvBuffer::dequeue(std::byte* dest, size_t len)
{
    auto avail_sz = availableDataSize();
    auto to_copy = std::min(static_cast<size_t>(avail_sz),len);
    auto firstBlockSz = std::min(sz_-(tail_%sz_),to_copy);
    auto firstBlockStart = buf_+(tail_%sz_);
    memcpy(dest, firstBlockStart, firstBlockSz);
    if (firstBlockSz < to_copy)
    {
        auto secondBlockSz = to_copy - firstBlockSz;
        memcpy(dest+firstBlockSz,buf_,secondBlockSz);
    }
    tail_ += to_copy;
    return to_copy;
}

uint32_t RecvBuffer::getAckNumber() const
{
    return nxt_;
}

uint16_t RecvBuffer::availableDataSize() const
{
    if (nxt_ == irs_+1) return 0;
    return (nxt_ + sz_ - tail_) % sz_;
}

uint16_t RecvBuffer::getWindowSize() const
{
    auto bLen = (nxt_ + sz_ - tail_) % sz_;
    return static_cast<uint16_t>(sz_ - bLen);
}