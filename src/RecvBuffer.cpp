#include <RecvBuffer.hpp>
#include <cstring>
#include <algorithm>
#include <iostream>

namespace ustacktcp {

RecvBuffer::RecvBuffer() {}

void RecvBuffer::setIRS(const uint32_t irs)
{
    ack_ = irs+1;
}

size_t RecvBuffer::getSize() const
{
    return (tail_ + sz_ - head_) % sz_;
}

size_t RecvBuffer::getAvailSize() const
{
    return sz_ - getSize() - 1;
}

ssize_t RecvBuffer::enqueue(const std::byte* data, const size_t len, const uint32_t seq_num)
{
    if (len > getAvailSize()) return -1; //FIXME: handle error

    auto firstBlockStart = buf_ + tail_;
    auto firstBlockSize = std::min(len, sz_-tail_);
    memcpy(firstBlockStart, data, firstBlockSize);
    if (firstBlockSize < len)
    {
        memcpy(buf_, data + firstBlockSize, len - firstBlockSize);
    }
    tail_ = (tail_ + len) % sz_;

    const auto p = std::make_shared<TCPSegment>(
        data,
        buf_,
        seq_num,
        len,
        firstBlockSize,
        TCPFlag::PSH
    );

    q_.push(p);
}

ssize_t RecvBuffer::dequeue(std::byte* dest, const size_t len)
{
    size_t copySz = 0;
    while (availableData() && q_.top()->len_ <= len - copySz)
    {
        const auto p = q_.top();
        memcpy(dest + copySz, p->data_, p->brk_len_);
        copySz += p->brk_len_;
        memcpy(dest + copySz, p->data2_, p->len_ - p->brk_len_);
        copySz += p->len_ - p->brk_len_;
        ack_ += p->len_;
        q_.pop();
    }
    return copySz;
}

uint32_t RecvBuffer::getAckNumber() const
{
    return ack_;
}

bool RecvBuffer::availableData() const
{
    return !q_.empty() && q_.top()->flags_ & TCPFlag::PSH && q_.top()->seq_start_ == ack_;
}

uint16_t RecvBuffer::getWindowSize() const
{
    return getAvailSize();
}

}