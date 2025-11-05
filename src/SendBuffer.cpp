#include <SendBuffer.hpp>

#include <cstring>
#include <algorithm>
#include <iostream>


SendBuffer::SendBuffer() {}

void SendBuffer::build(uint32_t iss)
{
    iss_ = iss;
    una_ = iss;
    nxt_ = iss + 1;
    wnd_ = 65535; // FIXME: initial window size
    head_ = iss+1;
    buf_ = new std::byte[sz_]; // FIXME: hardcoded max size
}

ssize_t SendBuffer::enqueue(const std::byte* data, size_t len)
{
    auto bLen = (head_ + sz_ - una_) % sz_;
    auto free_sz = sz_ - bLen;
    if (free_sz < len) return -1; // FIXME: add error code
    auto firstBlockSz = std::min(sz_-(head_%sz_),len);
    auto firstBlockStart = buf_+(head_%sz_);
    memcpy(firstBlockStart, data, firstBlockSz);
    if (firstBlockSz < len)
    {
        auto secondBlockSz = len - firstBlockSz;
        memcpy(buf_,data+firstBlockSz,secondBlockSz);
    }
    head_ += len;
    return len;
}

uint32_t SendBuffer::getSeqNumber() const
{
    return nxt_;
}

void SendBuffer::ack(uint32_t ack_num)
{
    if (ack_num <= una_) return;
    una_ = ack_num;
}

void SendBuffer::setWindowSize(uint16_t wnd) { wnd_ = wnd; }

std::byte* SendBuffer::getData(size_t& len)
{
    // TODO: handle wrap-around
    len = (head_ + sz_ - nxt_) % sz_;
    auto start = buf_ + (nxt_ % sz_);
    nxt_ += len;
    return start;
}

