#include <RecvBuffer.hpp>
#include <cstring>
#include <algorithm>

RecvBuffer::RecvBuffer() {}

void RecvBuffer::build(uint32_t irs)
{
    irs_ = irs;
    tail_ = irs;
    nxt_ = irs + 1;
    buf_ = new std::byte[sz_]; // FIXME: hardcoded max size
}

ssize_t RecvBuffer::enqueue(uint32_t seq_num, std::byte* data, size_t len)
{
    if (seq_num < nxt_) // already received
        return 0;
    if (seq_num > nxt_) // out of order
        return -1; // FIXME: add error code
    auto bLen = (tail_ + sz_ - nxt_) % sz_;
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

uint32_t RecvBuffer::getAckNumber() const
{
    return nxt_;
}

uint16_t RecvBuffer::availableDataSize() const
{
    return (nxt_ + sz_ - tail_) % sz_;
}