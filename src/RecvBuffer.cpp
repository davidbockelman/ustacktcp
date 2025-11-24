#include <RecvBuffer.hpp>
#include <cstring>
#include <algorithm>
#include <iostream>

namespace ustacktcp {

RecvBuffer::RecvBuffer() 
{
    tail_ = 0;
    head_ = 0;
    buf_ = new std::byte[sz_];
}

void RecvBuffer::setIRS(const uint32_t irs)
{
    ack_ = irs;
}

size_t RecvBuffer::getSize() const
{
    return (tail_ + sz_ - head_) % sz_;
}

size_t RecvBuffer::getAvailSize() const
{
    return sz_ - getSize() - 1;
}

ssize_t RecvBuffer::enqueue(const std::byte* data, const size_t len, const uint32_t seq_num, const uint8_t flags)
{
    if (len > getAvailSize()) return -1; //FIXME: handle error

    uint32_t s = seq_num, e = s + len;
    if (flags & TCPFlag::SYN || flags & TCPFlag::FIN) e++;
    size_t new_len = len;
    std::byte* new_data = const_cast<std::byte*>(data);

    auto it = q_.lower_bound(s);
    if (it != q_.begin()) --it;

    while (it != q_.end())
    {
        uint32_t es = it->second->seq_start_, ee = es + it->second->len_;
        if (it->second->flags_ & TCPFlag::SYN || it->second->flags_ & TCPFlag::FIN) ee++;

        if (SEQ_LEQ(ee, s))
        {
            ++it;
            continue;
        }
        if (SEQ_GEQ(es, e)) break;

        if (SEQ_LEQ(es, s) && SEQ_GEQ(ee, e)) return len;
        if (SEQ_LEQ(es, s) && SEQ_GT(ee, e))
        {
            uint32_t delta = ee - s;
            s   += delta;
            new_len -= delta;
            if (new_len > 0) new_data += delta;
            ++it;
            continue;
        }
        if (SEQ_GT(es, s) && SEQ_LT(es, e))
        {
            new_len = es - s;
            e   = s + new_len;
            break;
        }

        ++it;
    }

    if (new_len == 0 && !(flags & TCPFlag::SYN) && !(flags & TCPFlag::FIN)) return len;

    auto firstBlockStart = buf_ + tail_;
    auto firstBlockSize = std::min(new_len, sz_-tail_);
    memcpy(firstBlockStart, data, firstBlockSize);
    if (firstBlockSize < new_len)
    {
        memcpy(buf_, data + firstBlockSize, new_len - firstBlockSize);
    }
    tail_ = (tail_ + new_len) % sz_;

    const auto p = std::make_shared<TCPSegment>(
        data,
        buf_,
        s,
        new_len,
        firstBlockSize,
        flags
    );
    
    auto [rit, inserted] = q_.emplace(s, p);
    if (!inserted) return -1; // TODO: handle error

    // cascading ack
    while (rit != q_.end() && rit->second->seq_start_ == ack_)
    {
        ack_ += rit->second->len_;
        if (rit->second->flags_ & TCPFlag::SYN || rit->second->flags_ & TCPFlag::FIN) ++ack_;
        ++rit;
    }
    
    return len;
}

ssize_t RecvBuffer::dequeue(std::byte* dest, const size_t len)
{
    size_t copySz = 0;
    while (availableData() && q_.begin()->second->len_ <= len - copySz)
    {
        const auto p = q_.begin()->second;
        memcpy(dest + copySz, p->data_, p->brk_len_);
        copySz += p->brk_len_;
        memcpy(dest + copySz, p->data2_, p->len_ - p->brk_len_);
        copySz += p->len_ - p->brk_len_;
        q_.erase(q_.begin());
    }
    return copySz;
}

uint32_t RecvBuffer::getAckNumber() const
{
    return ack_;
}

bool RecvBuffer::availableData()
{
    auto first = q_.begin();
    if (!q_.empty() && first->second->flags_ & TCPFlag::SYN && first->second->seq_start_ < ack_) first = q_.erase(first); // remove initial syn
    return !q_.empty() && !(first->second->flags_ & TCPFlag::SYN) && !(first->second->flags_ & TCPFlag::FIN) && first->second->seq_start_ < ack_;
}

uint16_t RecvBuffer::getWindowSize() const
{
    return getAvailSize();
}

}