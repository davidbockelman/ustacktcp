#include <iostream> // TODO: trim includes
#include <unordered_map>
#include <memory>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip.h> 
#include <arpa/inet.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <net/if.h>

#include <TCPEngine.hpp>
#include <RecvBuffer.hpp>
#include <StreamSocket.hpp>

namespace ustacktcp {

std::shared_ptr<StreamSocket> make_socket(TCPEngine& engine)
{
    const auto ptr = std::make_shared<StreamSocket>(engine);
    engine.sockets_.push_back(ptr);
    return ptr;
}
    
TCPEngine::TCPEngine()
{
    _raw_fd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (_raw_fd < 0)
    {
        perror("TCPEngine::socket");
        exit(1);
    }
    std::thread t(&TimerManager::timeoutLoop, &timer_);
    t.detach();
}

bool TCPEngine::bind(const SocketAddr& addr, std::shared_ptr<StreamSocket> socket) {
    if (bound.find(addr) != bound.end()) {
        return false;
    }
    timer_.insertSocket(socket);
    bound[addr] = socket;
    return true;
}

ssize_t TCPEngine::send(std::shared_ptr<TCPSegment>& seg, const SocketAddr& src_addr, const SocketAddr& dest_addr, const RecvBuffer& recv_buf)
{
    // TODO: check socket state
    auto nonconst = const_cast<std::byte*>(seg->data_);
    TCPHeader* tcphdr = reinterpret_cast<TCPHeader*>(nonconst);
    tcphdr->src_port = htons(src_addr.port);
    tcphdr->dst_port = htons(dest_addr.port);
    tcphdr->seq_num = htonl(seg->seq_start_);
    tcphdr->ack_num = htonl(recv_buf.getAckNumber()); 
    tcphdr->data_offset = (sizeof(TCPHeader) / 4) << 4;
    tcphdr->flags = seg->flags_;
    tcphdr->window_size = htons(recv_buf.getWindowSize()); 
    tcphdr->checksum = 0;
    tcphdr->urgent_pointer = 0;

    PseudoIPv4Header iphdr;
    iphdr.src_addr = htonl(src_addr.ip.addr);
    iphdr.dst_addr = htonl(dest_addr.ip.addr);
    iphdr.zero = 0;
    iphdr.protocol = IPPROTO_TCP;
    iphdr.tcp_length = htons(seg->len_);

    InternetChecksumBuilder chksum;
    chksum.add(&iphdr, sizeof(PseudoIPv4Header));
    chksum.add(seg->data_, seg->len_);
    tcphdr->checksum = htons(chksum.finalize());

    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = htonl(dest_addr.ip.addr);

    seg->send_tmstp_ = std::chrono::steady_clock::now();
    if (sendto(_raw_fd,
                seg->data_, seg->len_,
                0,
                (sockaddr*)&dst, sizeof(dst))
        < 0)
        {
            perror("TCPEngine::sendto");
            return -1;
        }
    return seg->len_;
}

bool validTCPPort(uint16_t port) {
    return port >= 40000 && port <= 40010;
}

void TCPEngine::recv() {
    std::byte buffer[65536];
    ssize_t data_size;
    while (data_size = recvfrom(_raw_fd, buffer, sizeof(buffer), 0, nullptr, nullptr))
    {
        if (data_size < 0) {
            perror("TCPEngine::recvfrom");
            return;
        }
        IPHeader ip_header(buffer);
        if (!ip_header.nextProtoIsTCP()) continue;

        TCPHeader tcphdr(buffer + ip_header.getHeaderLength());

        // TODO: validate checksum
        // TODO: validate ports
        if (!validTCPPort(tcphdr.src_port) && !validTCPPort(tcphdr.dst_port)) continue;
        if (ip_header.getVersion() != 4) continue;

        size_t tcphdr_sz = tcphdr.data_offset >> 2;
        size_t payload_len = data_size - ip_header.getHeaderLength() - tcphdr_sz;
        SocketAddr dst_addr(IPAddr(ip_header.dst_addr), tcphdr.dst_port);
        SocketAddr src_addr(IPAddr(ip_header.src_addr), tcphdr.src_port);

        if (bound.find(dst_addr) == bound.end()) continue;

        auto sock = bound[dst_addr];
        
        auto flags = sock->handleCntrl(tcphdr, src_addr, payload_len);

        if (!flags) continue; // packet was dropped

        bool consumes_seq = (tcphdr.flags & TCPFlag::SYN) || (tcphdr.flags & TCPFlag::FIN) || payload_len > 0;
        
        if (consumes_seq)
        {
            sock->_recv_buffer.enqueue(buffer + ip_header.getHeaderLength() + tcphdr_sz, payload_len, tcphdr.seq_num, tcphdr.flags);
            sock->cv_.notify_all();
        }

        uint8_t res_flags = *flags;
        if (res_flags == 0) continue; // no response

        bool response_consumes_seq = (res_flags & TCPFlag::SYN) || (res_flags & TCPFlag::FIN); // never responding with data
        if (response_consumes_seq)
        {
            sock->_send_buffer.enqueue(nullptr, 0, res_flags);
            continue;
        }

        // response does not consume seq num (i.e. ack)
        TCPHeader tmp;

        auto p = std::make_shared<TCPSegment>(
            reinterpret_cast<std::byte*>(&tmp),
            nullptr,
            sock->_send_buffer.getSeqNumber(),
            sizeof(TCPHeader),
            sizeof(TCPHeader),
            res_flags
        );

        send(p, sock->_local_addr, sock->_peer_addr, sock->_recv_buffer);
    }
}

}