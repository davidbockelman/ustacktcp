#include <iostream>
#include <unordered_map>
#include <memory>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip.h>     // struct iphdr
#include <arpa/inet.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <thread>
#include <net/if.h>

#include <TCPEngine.hpp>
#include <StreamSocket.hpp>

using namespace ustacktcp;

int main() {

    TCPEngine engine;
    std::thread reciever(&TCPEngine::recv, &engine);
    StreamSocket socket(engine);
    StreamSocket socket2(engine);
    SocketAddr local_addr(IPAddr(inet_addr("127.0.0.1")), 8001);
    SocketAddr peer_addr(IPAddr(inet_addr("127.0.0.1")), 8000);
    socket.bind(local_addr);
    socket2.bind(peer_addr);
    socket2.listen();
    socket.connect(peer_addr);
    reciever.join();

    return 0;
}
