#include <StreamSocket.hpp>

#include <iostream>
#include <arpa/inet.h>

namespace ustacktcp {

Frame StreamSocket::createSYNFrame(const SocketAddr& dest_addr)
{
    TCPHeader tcphdr{};
    tcphdr.src_port = htons(_local_addr.port);
    tcphdr.dst_port = htons(dest_addr.port);
    tcphdr.seq_num = htonl(_iss);
    tcphdr.ack_num = 0;
    tcphdr.data_offset = (sizeof(TCPHeader) / 4) << 4;
    tcphdr.flags = TCPFlag::SYN;
    tcphdr.window_size = htons(65535); // FIXME: proper window size
    tcphdr.checksum = 0;
    tcphdr.urgent_pointer = 0;

    PseudoIPv4Header iphdr(_local_addr.ip.addr, _peer_addr.ip.addr, sizeof(TCPHeader));

    TCPData payload(nullptr, 0);

    Frame frame(iphdr, tcphdr, payload);
    return frame;
}

Frame StreamSocket::createSYNACKFrame(const SocketAddr& dest_addr, uint32_t ack_num)
{
    TCPHeader tcphdr{};
    tcphdr.src_port = htons(_local_addr.port);
    tcphdr.dst_port = htons(dest_addr.port);
    tcphdr.seq_num = htonl(_iss);
    tcphdr.ack_num = htonl(ack_num);
    tcphdr.data_offset = (sizeof(TCPHeader) / 4) << 4;
    tcphdr.flags = TCPFlag::SYN | TCPFlag::ACK;
    tcphdr.window_size = htons(65535); // FIXME: proper window size
    tcphdr.checksum = 0;
    tcphdr.urgent_pointer = 0;

    PseudoIPv4Header iphdr(_local_addr.ip.addr, _peer_addr.ip.addr, sizeof(TCPHeader));

    TCPData payload(nullptr, 0);

    Frame frame(iphdr, tcphdr, payload);
    return frame;
}

Frame StreamSocket::createACKFrame(const SocketAddr& dest_addr, uint32_t ack_num)
{
    TCPHeader tcphdr{};
    tcphdr.src_port = htons(_local_addr.port);
    tcphdr.dst_port = htons(dest_addr.port);
    tcphdr.seq_num = htonl(_iss);
    tcphdr.ack_num = htonl(ack_num);
    tcphdr.data_offset = (sizeof(TCPHeader) / 4) << 4;
    tcphdr.flags = TCPFlag::ACK;
    tcphdr.window_size = htons(65535); // FIXME: proper window size
    tcphdr.checksum = 0;
    tcphdr.urgent_pointer = 0;

    PseudoIPv4Header iphdr(_local_addr.ip.addr, _peer_addr.ip.addr, sizeof(TCPHeader));

    TCPData payload(nullptr, 0);

    Frame frame(iphdr, tcphdr, payload);
    return frame;
}

// FIXME: delete this constructor and use factory method
StreamSocket::StreamSocket(TCPEngine& engine) : _engine(engine) {}


bool StreamSocket::bind(const SocketAddr& addr)
{
    _local_addr = addr;
    return _engine.bind(addr, this);
}

bool StreamSocket::connect(const SocketAddr& addr)
{
    // Send SYN
    Frame syn_frame = createSYNFrame(addr);
    syn_frame.build();
    syn_frame.computeAndWriteChecksum();
    ssize_t sent_bytes = _engine.send(this, syn_frame);
    _state = SocketState::SYN_SENT;

    // Wait for SYN or SYN-ACK
    // SYN -> SYN_RECEIVED
    // SYN-ACK -> ESTABLISHED
    return true;
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
    if (_state != SocketState::LISTEN && _state != SocketState::SYN_SENT && _state != SocketState::SYN_RECEIVED)
    {
        std::cout << "Not in LISTEN, SYN_SENT, or SYN_RECEIVED state, ignoring segment." << std::endl;
        return;
    }

    if (segment.header.flags == TCPFlag::SYN)
    {
        std::cout << "Received SYN, transitioning to SYN_RECEIVED." << std::endl;
        _state = SocketState::SYN_RECEIVED;
        // Send SYN-ACK
        uint32_t ack_num = segment.header.seq_num + 1;
        Frame synack_frame = createSYNACKFrame(src_addr, ack_num);
        synack_frame.build();
        synack_frame.computeAndWriteChecksum();
        _engine.send(this, synack_frame);
    }

    if (segment.header.flags == (TCPFlag::SYN | TCPFlag::ACK))
    {
        std::cout << "Received SYN-ACK, transitioning to ESTABLISHED." << std::endl;
        _state = SocketState::ESTABLISHED;
        // Send ACK
        uint32_t ack_num = segment.header.seq_num + 1;
        Frame ack_frame = createACKFrame(src_addr, ack_num);
        ack_frame.build();
        ack_frame.computeAndWriteChecksum();
        _engine.send(this, ack_frame);
    }

    if (segment.header.flags == TCPFlag::ACK)
    {
        if (_state == SocketState::SYN_RECEIVED)
        {
            std::cout << "Received ACK in SYN_RECEIVED, transitioning to ESTABLISHED." << std::endl;
            _state = SocketState::ESTABLISHED;
        }
    }

}

}