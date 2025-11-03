#include <iostream> // TODO: trim includes
#include <unordered_map>
#include <memory>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip.h>     // struct iphdr
#include <arpa/inet.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <net/if.h>

#include <TCPEngine.hpp>

namespace ustacktcp {

TCPEngine::TCPEngine() 
{
    _raw_fd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (_raw_fd < 0)
    {
        perror("TCPEngine::socket");
        exit(1);
    }
}

bool TCPEngine::bind(const SocketAddr& addr, StreamSocket* socket) {
    if (bound.find(addr) != bound.end()) {
        return false;
    }
    bound[addr] = socket;
    return true;
}

ssize_t TCPEngine::send(const StreamSocket* sock, const Frame& frame)
{
    // TODO: check socket state
    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = htonl(frame.getDestinationIP());
    if (sendto(_raw_fd,
                frame.getTCPSegmentBuffer(), frame.getTCPSegmentLength(),
                0,
                (sockaddr*)&dst, sizeof(dst))
        < 0)
        {
            perror("TCPEngine::sendto");
            return -1;
        }
    return frame.getTCPSegmentLength();
}

bool validTCPPort(uint16_t port) {
    return port == 8000 || port == 8001;
}

void TCPEngine::recv() {
    unsigned char buffer[65536];
    ssize_t data_size;
    while (data_size = recvfrom(_raw_fd, buffer, sizeof(buffer), 0, nullptr, nullptr))
    {
        if (data_size < 0) {
            perror("TCPEngine::recvfrom");
            return;
        }
        IPHeader ip_header(buffer);
        if (!ip_header.nextProtoIsTCP()) {
            // Not a TCP packet
            // std::cout << "Non-TCP packet received, skipping." << std::endl;
            continue;
        }
        TCPHeader tcp_header(buffer + ip_header.getHeaderLength());
        // TODO: validate checksum
        // TODO: validate ports
        if (!validTCPPort(tcp_header.src_port) || !validTCPPort(tcp_header.dst_port)) {
            // Not for us
            // std::cout << "TCP packet received for invalid port " << tcp_header.src_port << " " << tcp_header.dst_port << ", skipping." << std::endl;
            continue;
        }
        size_t payload_len = data_size - ip_header.getHeaderLength() - sizeof(TCPHeader);
        const unsigned char* payload = new unsigned char[payload_len];
        memcpy((void*)payload, buffer + ip_header.getHeaderLength() + sizeof(TCPHeader), payload_len);
        TCPData tcp_data(payload, payload_len);
    
        // FIXME: pass options
        TCPSegment segment(tcp_header, TCPOptions(), tcp_data);
        SocketAddr dst_addr(IPAddr(ip_header.dst_addr), tcp_header.dst_port);
        SocketAddr src_addr(IPAddr(ip_header.src_addr), tcp_header.src_port);

        if (bound.find(dst_addr) == bound.end()) {
            // No socket bound to this address
            std::cout << "No socket bound to "
                      << inet_ntoa(*(in_addr*)&ip_header.dst_addr)
                      << ":" << tcp_header.dst_port
                      << std::endl;
            continue;
        }

        std::cout << "Received packet from "
                    << inet_ntoa(*(in_addr*)&ip_header.src_addr)
                    << ":" << tcp_header.src_port
                    << " to "
                    << inet_ntoa(*(in_addr*)&ip_header.dst_addr)
                    << ":" << tcp_header.dst_port
                    << " of size " << data_size << " bytes."
                    << std::endl;

        std::thread worker(&StreamSocket::handleSegment, bound[dst_addr], std::ref(segment), std::ref(src_addr));
        worker.detach();

        // Process the received TCP segment in 'buffer' of size 'data_size'
    }
}

}