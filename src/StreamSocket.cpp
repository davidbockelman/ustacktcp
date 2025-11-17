#include <StreamSocket.hpp>

#include <iostream>
#include <arpa/inet.h>

namespace ustacktcp {

Frame StreamSocket::createSYNFrame(const SocketAddr& dest_addr)
{
    TCPHeader tcphdr{};
    tcphdr.src_port = _local_addr.port;
    tcphdr.dst_port = dest_addr.port;
    tcphdr.seq_num = _iss;
    tcphdr.ack_num = 0;
    tcphdr.data_offset = (sizeof(TCPHeader) / 4) << 4;
    tcphdr.flags = TCPFlag::SYN;
    tcphdr.window_size = 65535; // FIXME: proper window size
    tcphdr.checksum = 0;
    tcphdr.urgent_pointer = 0;

    PseudoIPv4Header iphdr(_local_addr.ip.addr, dest_addr.ip.addr, sizeof(TCPHeader));

    TCPData payload(nullptr, 0);

    Frame frame(iphdr, tcphdr, payload);
    return frame;
}

Frame StreamSocket::createSYNACKFrame(const SocketAddr& dest_addr, uint32_t ack_num)
{
    TCPHeader tcphdr{};
    tcphdr.src_port = _local_addr.port;
    tcphdr.dst_port = dest_addr.port;
    tcphdr.seq_num = _iss;
    tcphdr.ack_num = ack_num;
    tcphdr.data_offset = (sizeof(TCPHeader) / 4) << 4;
    tcphdr.flags = TCPFlag::SYN | TCPFlag::ACK;
    tcphdr.window_size = _recv_buffer.getWindowSize(); // FIXME: proper window size
    tcphdr.checksum = 0;
    tcphdr.urgent_pointer = 0;

    PseudoIPv4Header iphdr(_local_addr.ip.addr, dest_addr.ip.addr, sizeof(TCPHeader));

    TCPData payload(nullptr, 0);

    Frame frame(iphdr, tcphdr, payload);
    return frame;
}

Frame StreamSocket::createACKFrame(const SocketAddr& dest_addr, uint32_t ack_num)
{
    TCPHeader tcphdr{};
    tcphdr.src_port = _local_addr.port;
    tcphdr.dst_port = dest_addr.port;
    tcphdr.seq_num = _send_buffer.getSeqNumber(); // FIXME: proper seq num
    tcphdr.ack_num = ack_num;
    tcphdr.data_offset = (sizeof(TCPHeader) / 4) << 4;
    tcphdr.flags = TCPFlag::ACK;
    tcphdr.window_size = _recv_buffer.getWindowSize(); // FIXME: proper window size
    tcphdr.checksum = 0;
    tcphdr.urgent_pointer = 0;

    PseudoIPv4Header iphdr(_local_addr.ip.addr, dest_addr.ip.addr, sizeof(TCPHeader));

    TCPData payload(nullptr, 0);

    Frame frame(iphdr, tcphdr, payload);
    return frame;
}

Frame StreamSocket::createDataFrame(const SocketAddr& dest_addr)
{
    size_t payload_len = 0;
    uint32_t seq_num = _send_buffer.getSeqNumber();
    std::byte* data_ptr = _send_buffer.getData(payload_len);
    if (payload_len == 0)
    {
        // No data to send
        return Frame(); // FIXME: handle properly
    }

    TCPHeader tcphdr{};
    tcphdr.src_port = _local_addr.port;
    tcphdr.dst_port = dest_addr.port;
    tcphdr.seq_num = seq_num;
    tcphdr.ack_num = _recv_buffer.getAckNumber();
    tcphdr.data_offset = (sizeof(TCPHeader) / 4) << 4;
    tcphdr.flags = TCPFlag::PSH | TCPFlag::ACK;
    tcphdr.window_size = _recv_buffer.getWindowSize(); // FIXME: proper window size
    tcphdr.checksum = 0;
    tcphdr.urgent_pointer = 0;

    PseudoIPv4Header iphdr(_local_addr.ip.addr, dest_addr.ip.addr, sizeof(TCPHeader) + payload_len);

    TCPData payload(data_ptr, payload_len);

    Frame frame(iphdr, tcphdr, payload);
    return frame;
}

Frame StreamSocket::createFINACKFrame(const SocketAddr& dest_addr)
{
    TCPHeader tcphdr{};
    tcphdr.src_port = _local_addr.port;
    tcphdr.dst_port = dest_addr.port;
    tcphdr.seq_num = _send_buffer.getSeqNumber(); // FIXME: proper seq num
    tcphdr.ack_num = _recv_buffer.getAckNumber();
    tcphdr.data_offset = (sizeof(TCPHeader) / 4) << 4;
    tcphdr.flags = TCPFlag::FIN | TCPFlag::ACK;
    tcphdr.window_size = _recv_buffer.getWindowSize(); // FIXME: proper window size
    tcphdr.checksum = 0;
    tcphdr.urgent_pointer = 0;

    PseudoIPv4Header iphdr(_local_addr.ip.addr, dest_addr.ip.addr, sizeof(TCPHeader));

    TCPData payload(nullptr, 0);

    Frame frame(iphdr, tcphdr, payload);
    return frame;
}

// FIXME: delete this constructor and use factory method
StreamSocket::StreamSocket(TCPEngine& engine) : _engine(engine), rtt_(std::chrono::milliseconds(1000)) {}


bool StreamSocket::bind(const SocketAddr& addr)
{
    _local_addr = addr;
    return _engine.bind(addr, this);
}

bool StreamSocket::connect(const SocketAddr& addr)
{
    // Send SYN
    _send_buffer.build(_iss); // FIXME: reset send buffer
    Frame syn_frame = createSYNFrame(addr);
    ssize_t sent_bytes = _engine.send(this, syn_frame);
    _state = SocketState::SYN_SENT;

    // Wait for SYN or SYN-ACK
    // SYN -> SYN_RECEIVED
    // SYN-ACK -> ESTABLISHED
    std::unique_lock<std::mutex> lock(m_);
    cv_.wait(lock, [this]() {
        return _state == SocketState::CLOSED || _state == SocketState::ESTABLISHED;
    });

    // FIXME: do i need to synchonize here?
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
        _engine.send(this, synack_frame);
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
        _engine.send(this, ack_frame);
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
        _engine.send(this, ack_frame);
    }

    if (segment.header.flags & TCPFlag::FIN)
    {
        Frame finack_frame = createFINACKFrame(src_addr);
        _engine.send(this, finack_frame);
    }

}

ssize_t StreamSocket::send(const std::byte* buf, size_t len)
{
    // Enqueue data into send buffer
    ssize_t enq_bytes = _send_buffer.enqueue(buf, len);
    if (enq_bytes < 0)
    {
        return -1; // FIXME: handle error
    }
    // Create data frame
    Frame data_frame = createDataFrame(_peer_addr);
    // Send data frame
    ssize_t sent_bytes = _engine.send(this, data_frame);
    return sent_bytes;
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