#include <types.hpp>

#include <iostream>
#include <arpa/inet.h>

namespace ustacktcp {

TCPHeader::TCPHeader(const unsigned char* buf)
{
    src_port = ntohs(*reinterpret_cast<const uint16_t*>(buf));
    dst_port = ntohs(*reinterpret_cast<const uint16_t*>(buf + 2));
    seq_num = ntohl(*reinterpret_cast<const uint32_t*>(buf + 4));
    ack_num = ntohl(*reinterpret_cast<const uint32_t*>(buf + 8));
    data_offset = buf[12];
    flags = buf[13];
    window_size = ntohs(*reinterpret_cast<const uint16_t*>(buf + 14));
    checksum = ntohs(*reinterpret_cast<const uint16_t*>(buf + 16));
    urgent_pointer = ntohs(*reinterpret_cast<const uint16_t*>(buf + 18));
}

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
    tcp_length(tcp_len) {}


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
    _sum = 0;
    const uint16_t* words = static_cast<const uint16_t*>(buf);
    while (len > 1)
    {
        _sum += *words++;
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

IPHeader::IPHeader(const unsigned char* buf)
{
    version_ihl = buf[0];
    tos = buf[1];
    total_length = ntohs(*reinterpret_cast<const uint16_t*>(buf + 2));
    identification = ntohs(*reinterpret_cast<const uint16_t*>(buf + 4));
    flags_fragment_offset = ntohs(*reinterpret_cast<const uint16_t*>(buf + 6));
    ttl = buf[8];
    protocol = buf[9];
    header_checksum = ntohs(*reinterpret_cast<const uint16_t*>(buf + 10));
    src_addr = *reinterpret_cast<const uint32_t*>(buf + 12);
    dst_addr = *reinterpret_cast<const uint32_t*>(buf + 16);
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