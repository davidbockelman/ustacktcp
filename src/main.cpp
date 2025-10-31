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
#include <net/if.h>

#include <TCPEngine.hpp>
#include <StreamSocket.hpp>

using namespace ustacktcp;

int main() {

    TCPEngine engine;
    StreamSocket socket(engine);
    SocketAddr local_addr(IPAddr(inet_addr("192.168.86.52")), htons(8080));
    socket.bind(local_addr);
    SocketAddr peer_addr(IPAddr(inet_addr("127.0.0.1")), htons(8000));
    socket.connect(peer_addr);

    return 0;
}
