#include <types.hpp>

#include <arpa/inet.h>

namespace ustacktcp {

ssize_t TCPHeader::write(unsigned char* buf) const
{
    memcpy(buf, &src_port, sizeof(src_port));
    memcpy(buf + 2, &dst_port, sizeof(dst_port));
    memcpy(buf + 4, &seq_num, sizeof(seq_num));
    memcpy(buf + 8, &ack_num, sizeof(ack_num));
    memcpy(buf + 12, &data_offset, sizeof(data_offset));
    memcpy(buf + 13, &flags, sizeof(flags));
    memcpy(buf + 14, &window_size, sizeof(window_size));
    memcpy(buf + 16, &checksum, sizeof(checksum));
    memcpy(buf + 18, &urgent_pointer, sizeof(urgent_pointer));
    return 20; // size of TCP header without options
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

PseudoIPv4Header::PseudoIPv4Header(uint32_t src, uint32_t dst, uint16_t tcp_len)
:   src_addr(src), 
    dst_addr(dst), 
    zero(0), 
    protocol(IPPROTO_TCP), 
    tcp_length(htons(tcp_len)) {}


ssize_t PseudoIPv4Header::write(unsigned char* buf) const
{
    memcpy(buf, &src_addr, sizeof(src_addr));
    memcpy(buf + 4, &dst_addr, sizeof(dst_addr));
    memcpy(buf + 8, &zero, sizeof(zero));
    memcpy(buf + 9, &protocol, sizeof(protocol));
    memcpy(buf + 10, &tcp_length, sizeof(tcp_length));
    return 12;
}

TCPData::TCPData(const unsigned char* p, size_t len)
:   payload(p), 
    payload_len(len) {}

ssize_t TCPData::write(unsigned char* buf) const
{
    memcpy(buf, payload, payload_len);
    return payload_len;
}

void InternetChecksumBuilder::add(const void* buf, size_t len)
{
    const uint16_t* words = static_cast<const uint16_t*>(buf);
    for (size_t i = 0; i < len / 2; ++i)
    {
        _sum += ntohs(words[i]);
    }
    if (len % 2)
    {
        _sum += ntohs(static_cast<const uint8_t*>(buf)[len - 1] << 8);
    }
}

uint16_t InternetChecksumBuilder::finalize()
{
    while (_sum >> 16)
    {
        _sum = (_sum & 0xFFFF) + (_sum >> 16);
    }
    return htons(static_cast<uint16_t>(~_sum));
}

Frame::Frame(PseudoIPv4Header& iphdr, TCPHeader& tcphdr, TCPData& payload)
:   _iphdr(iphdr), 
    _tcphdr(tcphdr), 
    _payload(payload), 
    _buf(nullptr), 
    _cap(0), 
    _len(0)
    {}

ssize_t Frame::build()
{
    _len = sizeof(PseudoIPv4Header) + sizeof(TCPHeader) + _payload.payload_len;
    _buf = new unsigned char[_len];
    if (!_buf) return -1; // FIXME: handle error

    size_t offset = 0;
    if (_iphdr.write(_buf) < 0) return -1; // FIXME: handle error
    offset += sizeof(PseudoIPv4Header);
    if (_tcphdr.write(_buf + offset) < 0) return -1; // FIXME: handle error
    offset += sizeof(TCPHeader);
    if (_payload.write(_buf + offset) < 0) return -1; // FIXME: handle error
    return _len;
}

int Frame::computeAndWriteChecksum()
{
    // TODO: handle error
    InternetChecksumBuilder checksum_builder;
    checksum_builder.add(_buf, _len);
    uint16_t checksum = checksum_builder.finalize();
    // Write checksum back to TCP header
    memcpy(_buf + sizeof(PseudoIPv4Header) + TCPHeader::getCheckSumOffset(), &checksum, sizeof(checksum));
    return 0;
}

size_t Frame::getTCPSegmentLength() const
{ 
    return sizeof(TCPHeader) + _payload.payload_len; 
}

const unsigned char* Frame::getTCPSegmentBuffer() const
{ 
    return _buf + sizeof(PseudoIPv4Header); 
}

const uint32_t Frame::getDestinationIP() const
{ 
    return _iphdr.dst_addr; 
}

}