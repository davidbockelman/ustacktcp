#include <iostream>
#include <unordered_map>
#include <memory>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <cstddef>
#include <netinet/ip.h>     // struct iphdr
#include <arpa/inet.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <thread>
#include <net/if.h>

#include <TCPEngine.hpp>
#include <StreamSocket.hpp>

using namespace ustacktcp;

void recvLoop(const std::shared_ptr<StreamSocket>& s)
{
    std::byte buf[65535];
    ssize_t data_sz;
    while (data_sz = s->recv(buf, 65535))
    {
        buf[data_sz-1] = std::byte {};
        const char* in = reinterpret_cast<const char*>(buf);
        std::cout << std::string(in, data_sz) << std::endl;
    }
}

void sendLoop(const std::shared_ptr<StreamSocket>& s)
{
    std::string line;
    while (std::getline(std::cin, line))
    {
        line.append(1, '\n');
        const char* data = line.data();
        size_t len = line.size();
        std::byte* buf = const_cast<std::byte*>(reinterpret_cast<const std::byte*>(data));
        s->send(buf, len);
    }
}

int main() {

    TCPEngine engine;
    std::thread reciever(&TCPEngine::recv, &engine);
    reciever.detach();
    auto socket = make_socket(engine);
    SocketAddr local_addr(IPAddr(ntohl(inet_addr("127.0.0.1"))), 40000);
    // SocketAddr peer_addr(IPAddr(ntohl(inet_addr("127.0.0.1"))), 40012);
    socket->bind(local_addr);
    socket->listen();

    std::thread recvThr(recvLoop, std::ref(socket));
    recvThr.detach();
    
    sendLoop(socket);

    return 0;
}
