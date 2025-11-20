#include <StreamSocket.hpp>

#include <iostream>
#include <arpa/inet.h>

namespace ustacktcp {

// FIXME: delete this constructor and use factory method
StreamSocket::StreamSocket(TCPEngine& engine) : _engine(engine), _send_buffer(engine, _recv_buffer) {}


bool StreamSocket::bind(const SocketAddr& addr)
{
    _local_addr = addr;
    _send_buffer.setLocalAddr(addr);
    return _engine.bind(addr, shared_from_this());
}

bool StreamSocket::connect(const SocketAddr& addr)
{
    // Send SYN
    _peer_addr = addr;
    _send_buffer.setPeerAddr(addr);
    _state = SocketState::SYN_SENT;
    _send_buffer.enqueue(nullptr, 0, TCPFlag::SYN);

    // Wait for SYN or SYN-ACK
    // SYN -> SYN_RECEIVED
    // SYN-ACK -> ESTABLISHED
    std::unique_lock<std::mutex> lock(m_);
    cv_.wait(lock, [this]() {
        return _state == SocketState::CLOSED || _state == SocketState::ESTABLISHED;
    });

    return _state == SocketState::ESTABLISHED;
}

bool StreamSocket::listen()
{
    _state = SocketState::LISTEN;
    return true;
}


ssize_t StreamSocket::send(const std::byte* buf, size_t len)
{
    // Enqueue data into send buffer
    ssize_t enq_bytes = _send_buffer.enqueue(buf, len, TCPFlag::PSH | TCPFlag::ACK);
    if (enq_bytes < 0)
    {
        return -1; // FIXME: handle error
    }
    return enq_bytes;
}

void StreamSocket::handleCntrl(const TCPHeader& tcphdr, const SocketAddr& src_addr)
{
    _send_buffer.setRcvWnd(tcphdr.window_size);
    
    if (tcphdr.flags == TCPFlag::SYN)
    {
        std::lock_guard lg(m_);
        _peer_addr = src_addr;
        _send_buffer.setPeerAddr(src_addr);
        _recv_buffer.setIRS(tcphdr.seq_num);
        // TODO: check state
        _state = SocketState::SYN_RECEIVED;
        _send_buffer.enqueue(nullptr, 0, TCPFlag::SYN & TCPFlag::ACK);
        return;
    }

    if (tcphdr.flags & TCPFlag::ACK)
    {
        _send_buffer.handleACK(tcphdr.ack_num, std::chrono::steady_clock::now());
        if (_state == SocketState::SYN_RECEIVED)
        {
            _state = SocketState::ESTABLISHED;
            cv_.notify_all();
        }
    }

    if (tcphdr.flags & TCPFlag::SYN)
    {
        _recv_buffer.setIRS(tcphdr.seq_num);
    }

    //TODO: make sure FIN works
    if (tcphdr.flags & TCPFlag::SYN || tcphdr.flags & TCPFlag::PSH || tcphdr.flags & TCPFlag::FIN)
    {
        TCPHeader tmp;
        auto p = std::make_shared<TCPSegment>(
            reinterpret_cast<const std::byte*>(&tmp),
            nullptr,
            _send_buffer.getSeqNumber(),
            sizeof(TCPHeader),
            sizeof(TCPHeader),
            TCPFlag::ACK
        );
        _engine.send(p, _local_addr, _peer_addr, _recv_buffer);
        if (_state == SocketState::SYN_SENT)
        {
            std::lock_guard lg(m_);
            _state = SocketState::ESTABLISHED;
        }
        //TODO: is the right place to wake up blocked recv calls?
        cv_.notify_all();
    }
    
}

ssize_t StreamSocket::recv(std::byte* buf, size_t len)
{
    std::unique_lock<std::mutex> lock(m_);
    cv_.wait(lock, [this]() {
        return _recv_buffer.availableData();
    });
    return _recv_buffer.dequeue(buf, len);
}



}