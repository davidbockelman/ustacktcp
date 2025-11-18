#include <SendBuffer.hpp>

#include <cstring>
#include <algorithm>
#include <iostream>

namespace ustacktcp {

size_t SendBuffer::getAvailSize() const
{
    return sz_ - getSize() - 1;
}
    
size_t SendBuffer::getSize() const
{
    return (tail_ + sz_ - head_) % sz_;
}
    
bool SendBuffer::isEmpty() const
{
    return tail_ == head_;
}
    
bool SendBuffer::isFull() const
{
    return (tail_ + 1) % sz_ == head_;
}

void SendBuffer::sendSegments()
{
    if (next_q_.empty()) return;
    
    //TODO: implement window checking
    // while (cond)
    std::shared_ptr<TCPSegment> p = next_q_.top();
    // send logic
    if (in_flight_q_.empty()) restartRTO();
    engine_.send(p, local_addr_, peer_addr_, recv_buf_);
    next_q_.pop();
    in_flight_q_.push(p);
}

SendBuffer::SendBuffer(TCPEngine& engine, RecvBuffer& recv_buf) 
:   engine_(engine),
    recv_buf_(recv_buf)
{
    buf_ = new std::byte[sz_]; // FIXME: hardcoded max size
    head_ = 0;
    tail_ = 0;
}

void SendBuffer::setLocalAddr(const SocketAddr& local_addr) { local_addr_ = local_addr; }

void SendBuffer::setPeerAddr(const SocketAddr& peer_addr) { peer_addr_ = peer_addr; }


ssize_t SendBuffer::enqueue(const std::byte* data, size_t len, const uint8_t flags)
{
    // TODO: allow options
    auto availSize = getAvailSize();
    auto headrSz = sizeof(TCPHeader);
    if (availSize < len + headrSz) return -1; //FIXME: handle error
    auto firstBlockStart = buf_ + tail_;
    auto firstBlockSize = std::min(headrSz,sz_-tail_);
    tail_ = (tail_ + headrSz) % sz_;
    auto secondBlockStart = buf_ + tail_;
    auto secondBlockSize = std::min(len,sz_-tail_);
    memcpy(secondBlockStart, data, secondBlockSize);

    size_t brk_len;
    if (firstBlockSize < headrSz) brk_len = firstBlockSize;
    else if (secondBlockSize < len)
    {
        brk_len = headrSz + secondBlockSize;
        memcpy(buf_, data + secondBlockSize, len - secondBlockSize);
    }
    else brk_len = len + headrSz;

    tail_ = (tail_ + len) % sz_;



    std::shared_ptr<TCPSegment> p = std::make_shared<TCPSegment>(
        firstBlockStart,
        buf_,
        next_seq_num_,
        len + headrSz,
        brk_len,
        flags
    );

    if (flags & TCPFlag::PSH) next_seq_num_ += len;
    else if (flags & TCPFlag::SYN || flags & TCPFlag::FIN) next_seq_num_++;

    next_q_.push(p);
    sendSegments();

    return len;
}

void SendBuffer::rttSample(const std::chrono::steady_clock::duration rtt)
{
    if (!rtt_init_)
    {
        srtt_ = rtt;
        rttvar_ = rtt / 2;
        rto_ = srtt_ + std::max(G, K * rttvar_);
        rto_ = std::clamp(rto_, RTO_MIN, RTO_MAX);
        rtt_init_ = true;
        return;
    }
    auto abs_diff = srtt_ < rtt ? rtt - srtt_ : srtt_ - rtt;
    rttvar_ = std::chrono::duration_cast<std::chrono::steady_clock::duration>((3.0/4.0) * rttvar_ + (1.0/4.0) * abs_diff);
    srtt_ = std::chrono::duration_cast<std::chrono::steady_clock::duration>((7.0/8.0) * srtt_ + (1.0/8.0) * rtt);
    rto_ = srtt_ + std::max(G, K * rttvar_);
    rto_ = std::clamp(rto_, RTO_MIN, RTO_MAX);
}

void SendBuffer::restartRTO()
{
    to_expiry_ = std::chrono::steady_clock::now() + rto_;
}

void SendBuffer::handleACK(const uint32_t ack_num, const std::chrono::steady_clock::time_point ack_timestmp)
{
    if (ack_num <= ack_num_) return;
    ack_num_ = ack_num;
    //TODO: implement binary search
    bool rtt_probed = false;
    auto prev_head = head_;
    while (!in_flight_q_.empty() && ack_num > in_flight_q_.top()->seq_start_)
    {
        auto cur = in_flight_q_.top();
        if (!rtt_probed && cur->retransmit_cnt_ == 0)
        {
            auto rtt_sample = ack_timestmp - cur->send_tmstp_;
            rttSample(rtt_sample);
            rtt_probed = true;
        }
        head_ = (head_ + cur->len_) % sz_;
        in_flight_q_.pop();
    }
    if (!in_flight_q_.empty() && ack_num != in_flight_q_.top()->seq_start_); // TODO: handle out of sync error

    if (prev_head != head_)
    {
        restartRTO();
        sendSegments();
    }
}

void SendBuffer::handleRTO()
{
    if (in_flight_q_.empty()) return;
    auto p = in_flight_q_.top();
    p->retransmit_cnt_++;
    restartRTO();
    // send logic
    engine_.send(p, local_addr_, peer_addr_, recv_buf_);
}

}