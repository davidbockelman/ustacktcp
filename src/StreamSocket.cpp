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

void StreamSocket::handleSegment(const TCPSegment& segment, const SocketAddr& src_addr)
{
    // Handle incoming segment based on current state
    // Update state accordingly
    if (_state != SocketState::LISTEN && _state != SocketState::SYN_SENT && _state != SocketState::SYN_RECEIVED && _state != SocketState::ESTABLISHED)
    {
        // std::cout << "Not in LISTEN, SYN_SENT, or SYN_RECEIVED state, ignoring segment." << std::endl;
        return;
    }

    if (segment.header.flags == TCPFlag::SYN)
    {
        // std::cout << "Received SYN, transitioning to SYN_RECEIVED." << std::endl;
        _peer_addr = src_addr;
        _state = SocketState::SYN_RECEIVED;
        _recv_buffer.build(segment.header.seq_num);
        // Send SYN-ACK
        uint32_t ack_num =  _recv_buffer.getAckNumber();
        Frame synack_frame = createSYNACKFrame(src_addr, ack_num);
        _engine.send(synack_frame);
    }

    if (segment.header.flags == (TCPFlag::SYN | TCPFlag::ACK))
    {
        // std::cout << "Received SYN-ACK, transitioning to ESTABLISHED." << std::endl;
        _peer_addr = src_addr;
        _recv_buffer.build(segment.header.seq_num);
        _send_buffer.ack(segment.header.ack_num);
        {
            std::lock_guard<std::mutex> lock(m_);
            _state = SocketState::ESTABLISHED;
            cv_.notify_all();
        }
        // Send ACK
        uint32_t ack_num = _recv_buffer.getAckNumber();
        Frame ack_frame = createACKFrame(src_addr, ack_num);
        _engine.send(ack_frame);
    }

    if (segment.header.flags == TCPFlag::ACK)
    {
        _send_buffer.ack(segment.header.ack_num);
        if (_state == SocketState::SYN_RECEIVED)
        {
            // std::cout << "Received ACK in SYN_RECEIVED, transitioning to ESTABLISHED." << std::endl;
            
            {
                std::lock_guard<std::mutex> lock(m_);
                _state = SocketState::ESTABLISHED;
                cv_.notify_all();
            }
        }
    }

    if (segment.header.flags & TCPFlag::PSH)
    {
        // std::cout << "Received PSH, enqueueing data." << std::endl;
        ssize_t enq_bytes;
        uint32_t ack_num;
        {
            std::lock_guard<std::mutex> lg(m_);
            enq_bytes = _recv_buffer.enqueue(segment.header.seq_num, reinterpret_cast<const std::byte*>(segment.data.payload), segment.data.payload_len);
            ack_num  = _recv_buffer.getAckNumber();
            if (_recv_buffer.availableDataSize() > 0)
                cv_.notify_all();
        } 
        // Send ACK for received data
        Frame ack_frame = createACKFrame(src_addr, ack_num);
        _engine.send(ack_frame);
    }

    if (segment.header.flags & TCPFlag::FIN)
    {
        Frame finack_frame = createFINACKFrame(src_addr);
        _engine.send(finack_frame);
    }

}

ssize_t StreamSocket::send(const std::byte* buf, size_t len)
{
    // Enqueue data into send buffer
    ssize_t enq_bytes = _send_buffer.enqueue(buf, len, TCPFlag::PSH);
    if (enq_bytes < 0)
    {
        return -1; // FIXME: handle error
    }
    return enq_bytes;
}

ssize_t StreamSocket::recv(std::byte* buf, size_t len)
{
    size_t available = _recv_buffer.availableDataSize();
    ssize_t copySz;
    if (available == 0)
    {
        // No data available, wait
        std::unique_lock<std::mutex> lock(m_);
        cv_.wait(lock, [this]() {
            return _recv_buffer.availableDataSize() > 0;
        });
        available = _recv_buffer.availableDataSize();
        copySz = std::min(len, available);
        copySz = _recv_buffer.dequeue(buf, copySz); // TODO: handle error
    }
    return copySz;
}



}