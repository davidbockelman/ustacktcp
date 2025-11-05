#include <types.hpp>

#include <iostream>
#include <arpa/inet.h>

namespace ustacktcp {

TCPHeader::TCPHeader(const std::byte* buf)
{
    src_port = ntohs(*reinterpret_cast<const uint16_t*>(buf));
    dst_port = ntohs(*reinterpret_cast<const uint16_t*>(buf + 2));
    seq_num = ntohl(*reinterpret_cast<const uint32_t*>(buf + 4));
    ack_num = ntohl(*reinterpret_cast<const uint32_t*>(buf + 8));
    data_offset = std::to_integer<uint8_t>(buf[12]);
    flags = std::to_integer<uint8_t>(buf[13]);
    window_size = ntohs(*reinterpret_cast<const uint16_t*>(buf + 14));
    checksum = ntohs(*reinterpret_cast<const uint16_t*>(buf + 16));
    urgent_pointer = ntohs(*reinterpret_cast<const uint16_t*>(buf + 18));
}

int TCPHeader::writeNetworkBytes(std::byte* buf) const
{
    uint16_t src_port_n = htons(src_port);
    uint16_t dst_port_n = htons(dst_port);
    uint32_t seq_num_n = htonl(seq_num);
    uint32_t ack_num_n = htonl(ack_num);
    uint16_t window_size_n = htons(window_size);
    uint16_t checksum_n = htons(checksum);
    uint16_t urgent_pointer_n = htons(urgent_pointer);

    memcpy(buf, &src_port_n, sizeof(src_port_n));
    memcpy(buf + 2, &dst_port_n, sizeof(dst_port_n));
    memcpy(buf + 4, &seq_num_n, sizeof(seq_num_n));
    memcpy(buf + 8, &ack_num_n, sizeof(ack_num_n));
    memcpy(buf + 12, &data_offset, sizeof(data_offset));
    memcpy(buf + 13, &flags, sizeof(flags));
    memcpy(buf + 14, &window_size_n, sizeof(window_size_n));
    memcpy(buf + 16, &checksum_n, sizeof(checksum_n));
    memcpy(buf + 18, &urgent_pointer_n, sizeof(urgent_pointer_n));
    return 0;
}

size_t TCPHeader::getCheckSumOffset()
{
    return 16; // offset of checksum field in TCP header
}

IPAddr::IPAddr(uint32_t a) : addr(a) {}

bool IPAddr::operator==(const IPAddr& other) const
{
    return addr == other.addr;
}

SocketAddr::SocketAddr(IPAddr i, uint16_t p) : ip(i), port(p) {}

SocketAddr::SocketAddr() : ip(IPAddr(0)), port(0) {}

bool SocketAddr::operator==(const SocketAddr& other) const
{
    return (ip == other.ip) && (port == other.port);
}

PseudoIPv4Header::PseudoIPv4Header() {};

PseudoIPv4Header::PseudoIPv4Header(uint32_t src, uint32_t dst, uint16_t tcp_len)
:   src_addr(src), 
    dst_addr(dst), 
    zero(0), 
    protocol(IPPROTO_TCP), 
    tcp_length(tcp_len) {}


int PseudoIPv4Header::writeNetworkBytes(std::byte* buf) const
{
    uint32_t src_addr_n = htonl(src_addr);
    uint32_t dst_addr_n = htonl(dst_addr);
    uint16_t tcp_length_n = htons(tcp_length);

    memcpy(buf, &src_addr_n, sizeof(src_addr_n));
    memcpy(buf + 4, &dst_addr_n, sizeof(dst_addr_n));
    memcpy(buf + 8, &zero, sizeof(zero));
    memcpy(buf + 9, &protocol, sizeof(protocol));
    memcpy(buf + 10, &tcp_length_n, sizeof(tcp_length_n));
    return 0;
}



TCPData::TCPData(const std::byte* p, size_t len)
:   payload(p), 
    payload_len(len) {}

int TCPData::writeNetworkBytes(std::byte* buf) const
{
    memcpy(buf, payload, payload_len);
    return 0;
}

void InternetChecksumBuilder::add(const void* buf, size_t len)
{
    _sum = 0;
    const uint16_t* words = reinterpret_cast<const uint16_t*>(buf);
    while (len > 1)
    {
        _sum += htons(*words++);
        if (_sum & 0xFFFF0000)
        {
            _sum = (_sum & 0xFFFF) + (_sum >> 16);
        }
        len -= 2;
    }
    if (len > 0)
    {
        _sum += *(reinterpret_cast<const uint8_t*>(words));
    }
    if (_sum & 0xFFFF0000)
    {
        _sum = (_sum & 0xFFFF) + (_sum >> 16);
    }
}

uint16_t InternetChecksumBuilder::finalize()
{
    return (uint16_t)(~_sum);
}

Frame::Frame() {}

Frame::Frame(PseudoIPv4Header& iphdr, TCPHeader& tcphdr, TCPData& payload)
:   _iphdr(iphdr), 
    _tcphdr(tcphdr), 
    _payload(payload), 
    _buf(nullptr), 
    _cap(0), 
    _len(0)
    {}

int Frame::writeNetworkBytes()
{
    _len = sizeof(PseudoIPv4Header) + sizeof(TCPHeader) + _payload.payload_len;
    _buf = new std::byte[_len];
    if (!_buf) return -1; // FIXME: handle error

    size_t offset = 0;
    if (_iphdr.writeNetworkBytes(_buf) < 0) return -1;
    offset += sizeof(PseudoIPv4Header);
    if (_tcphdr.writeNetworkBytes(_buf + offset) < 0) return -1;
    offset += sizeof(TCPHeader);
    if (_payload.writeNetworkBytes(_buf + offset) < 0) return -1;
    return 0;
}

int Frame::computeAndWriteChecksum()
{
    // TODO: handle error
    InternetChecksumBuilder checksum_builder;
    checksum_builder.add(_buf, _len);
    uint16_t checksum = htons(checksum_builder.finalize());
    // Write checksum back to TCP header
    memcpy(_buf + sizeof(PseudoIPv4Header) + TCPHeader::getCheckSumOffset(), &checksum, sizeof(checksum));
    return 0;
}

size_t Frame::getTCPSegmentLength() const
{ 
    return sizeof(TCPHeader) + _payload.payload_len; 
}

const std::byte* Frame::getTCPSegmentBuffer() const
{ 
    return _buf + sizeof(PseudoIPv4Header); 
}

const uint32_t Frame::getDestinationIP() const
{ 
    return _iphdr.dst_addr; 
}

IPHeader::IPHeader(const std::byte* buf)
{
    version_ihl = std::to_integer<uint8_t>(buf[0]);
    tos = std::to_integer<uint8_t>(buf[1]);
    total_length = ntohs(*reinterpret_cast<const uint16_t*>(buf + 2));
    identification = ntohs(*reinterpret_cast<const uint16_t*>(buf + 4));
    flags_fragment_offset = ntohs(*reinterpret_cast<const uint16_t*>(buf + 6));
    ttl = std::to_integer<uint8_t>(buf[8]);
    protocol = std::to_integer<uint8_t>(buf[9]);
    header_checksum = ntohs(*reinterpret_cast<const uint16_t*>(buf + 10));
    src_addr = ntohl(*reinterpret_cast<const uint32_t*>(buf + 12));
    dst_addr = ntohl(*reinterpret_cast<const uint32_t*>(buf + 16));
}

bool IPHeader::nextProtoIsTCP() const
{
    return protocol == IPPROTO_TCP;
}

size_t IPHeader::getHeaderLength() const
{
    return (version_ihl & 0x0F) * 4;
}

// FIXME: copying
TCPSegment::TCPSegment(TCPHeader h, TCPOptions o, TCPData d)
{
    header = h;
    options = o;
    data = d;
}

}