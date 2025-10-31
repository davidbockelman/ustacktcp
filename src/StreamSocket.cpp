#include <StreamSocket.hpp>

#include <arpa/inet.h>

namespace ustacktcp {

Frame StreamSocket::createSYNFrame(const SocketAddr& dest_addr)
{
    TCPHeader tcphdr{};
    tcphdr.src_port = _local_addr.port;
    tcphdr.dst_port = dest_addr.port;
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

    // Wait for SYN or SYN-ACK
    // SYN -> SYN_RECEIVED
    // SYN-ACK -> ESTABLISHED
    return true;
}

}