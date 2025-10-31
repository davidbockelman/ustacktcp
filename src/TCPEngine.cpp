#include <iostream> // TODO: trim includes
#include <unordered_map>
#include <memory>
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

}