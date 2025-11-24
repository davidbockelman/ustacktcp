#include <StreamSocket.hpp>

#include <iostream>
#include <arpa/inet.h>

namespace ustacktcp {

bool StreamSocket::validSeqNum(uint32_t seq_start, size_t len) const
{
    uint32_t seq_end = seq_start + len - 1;
    uint32_t rcv_start = _recv_buffer.getAckNumber(), rcv_end = rcv_start + _recv_buffer.getWindowSize() -1;
    // std::cout << "new [" << seq_start << "," << seq_end << "] wnd [" << rcv_start << "," << rcv_end << "]" << std::endl;
    return (SEQ_LEQ(rcv_start,seq_start) && SEQ_LEQ(seq_start,rcv_end)) || (SEQ_LEQ(rcv_start,seq_end) && SEQ_LEQ(seq_end,rcv_end));
}

bool StreamSocket::validFlags(uint8_t flags) const
{
    if (flags == 0) return false;
    SocketState s;
    s = _state;
    // Extract only control bits
    uint8_t f = flags & (TCPFlag::SYN | TCPFlag::ACK | TCPFlag::FIN | TCPFlag::RST);

    switch (s)
    {
        case SocketState::LISTEN:
            return f == TCPFlag::SYN;

        case SocketState::SYN_SENT:
            // Valid:
            // SYN        (simultaneous open)
            // SYN|ACK    (normal handshake)
            // ACK        (acceptable if ACKs something valid)
            // RST        (abort from peer)
            return
                f == TCPFlag::SYN ||
                f == (TCPFlag::SYN | TCPFlag::ACK) ||
                // TODO: should this be allowed? f == TCPFlag::ACK || 
                f == TCPFlag::RST ||
                f == (TCPFlag::RST | TCPFlag::ACK);

        case SocketState::SYN_RECEIVED:
            // Valid:
            // ACK        (handshake completion)
            // FIN        (rare but legal)
            // RST        (abort)
            // RST|ACK
            return
                f == TCPFlag::ACK ||
                f == TCPFlag::FIN ||
                f == TCPFlag::RST ||
                f == (TCPFlag::RST | TCPFlag::ACK);

        case SocketState::ESTABLISHED:
            // Valid:
            // ACK
            // FIN|ACK
            // RST
            // RST|ACK
            return
                f == TCPFlag::ACK ||
                f == (TCPFlag::FIN | TCPFlag::ACK) ||
                f == TCPFlag::RST ||
                f == (TCPFlag::RST | TCPFlag::ACK);

        case SocketState::FIN_WAIT_1:
            // Valid:
            // ACK
            // FIN|ACK
            // FIN        (remote closing early before ACKing our FIN)
            // RST
            // RST|ACK
            return
                f == TCPFlag::ACK ||
                f == (TCPFlag::FIN | TCPFlag::ACK) ||
                f == TCPFlag::FIN ||
                f == TCPFlag::RST ||
                f == (TCPFlag::RST | TCPFlag::ACK);

        case SocketState::FIN_WAIT_2:
            // Valid:
            // ACK
            // FIN|ACK
            // RST
            // RST|ACK
            return
                f == TCPFlag::ACK ||
                f == (TCPFlag::FIN | TCPFlag::ACK) ||
                f == TCPFlag::RST ||
                f == (TCPFlag::RST | TCPFlag::ACK);

        case SocketState::CLOSE_WAIT:
            // Valid:
            // ACK
            // RST
            // RST|ACK
            return
                f == TCPFlag::ACK ||
                f == TCPFlag::RST ||
                f == (TCPFlag::RST | TCPFlag::ACK);

        case SocketState::CLOSING:
            // Valid:
            // ACK
            // RST
            // RST|ACK
            return
                f == TCPFlag::ACK ||
                f == TCPFlag::RST ||
                f == (TCPFlag::RST | TCPFlag::ACK);

        case SocketState::LAST_ACK:
            // Valid:
            // ACK
            // RST
            // RST|ACK
            return
                f == TCPFlag::ACK ||
                f == TCPFlag::RST ||
                f == (TCPFlag::RST | TCPFlag::ACK);

        case SocketState::TIME_WAIT:
            // Valid:
            // ACK
            // FIN|ACK   (duplicate FINs)
            // RST
            // RST|ACK
            return
                f == TCPFlag::ACK ||
                f == (TCPFlag::FIN | TCPFlag::ACK) ||
                f == TCPFlag::RST ||
                f == (TCPFlag::RST | TCPFlag::ACK);

        default:
            return false;
    }
}

// FIXME: delete this constructor and use factory method
StreamSocket::StreamSocket(TCPEngine& engine) : _engine(engine), _send_buffer(engine, _recv_buffer) {}


bool StreamSocket::bind(const SocketAddr& addr)
{
    _local_addr = addr;
    _send_buffer.setLocalAddr(addr);
    return _engine.bind(addr, shared_from_this());
}

bool StreamSocket::connect(const SocketAddr& addr)
{
    // Send SYN
    
    // Wait for SYN or SYN-ACK
    // SYN -> SYN_RECEIVED
    // SYN-ACK -> ESTABLISHED
    std::unique_lock<std::mutex> lock(m_);
    if (_state != SocketState::CLOSED) return false; // TODO: handle error
    _peer_addr = addr;
    _send_buffer.setPeerAddr(addr);
    _state = SocketState::SYN_SENT;
    _send_buffer.enqueue(nullptr, 0, TCPFlag::SYN);
    cv_.wait(lock, [this]() {
        return _state == SocketState::CLOSED || _state == SocketState::ESTABLISHED;
    });

    return _state == SocketState::ESTABLISHED;
}

bool StreamSocket::listen()
{
    std::unique_lock<std::mutex> lock(m_);
    if (_state != SocketState::CLOSED) return false; // TODO: handle error
    _state = SocketState::LISTEN;
    cv_.wait(lock, [this]() {
        return _state == SocketState::CLOSED || _state == SocketState::ESTABLISHED;
    });
    return _state == SocketState::ESTABLISHED;
}


ssize_t StreamSocket::send(const std::byte* buf, size_t len)
{
    // Enqueue data into send buffer
    ssize_t enq_bytes;
    {
        std::lock_guard lock(m_);
        enq_bytes = _send_buffer.enqueue(buf, len, TCPFlag::PSH | TCPFlag::ACK);
    }
    if (enq_bytes < 0)
    {
        return -1; // FIXME: handle error
    }
    return enq_bytes;
}

std::optional<uint8_t> StreamSocket::handleCntrl(const TCPHeader& tcphdr, const SocketAddr& src_addr, const size_t data_len)
{
    bool flags_ok = validFlags(tcphdr.flags);
    bool seq_ok = ((_state == SocketState::LISTEN || _state == SocketState::SYN_SENT || _state == SocketState::SYN_RECEIVED) && flags_ok) || validSeqNum(tcphdr.seq_num, data_len);
    if (!seq_ok) return std::nullopt; //TODO: handle error
    if (!flags_ok || (tcphdr.flags & TCPFlag::RST))
    {
        {
            std::lock_guard lock(m_);
            _state = SocketState::CLOSED;
        }
        // TODO: send RST
        // TODO: set error and wake blocked threads
        return TCPFlag::RST; 
    }

    SocketState s;
    {
        std::lock_guard lock(m_);
        s = _state;
    }

    if (s != SocketState::LISTEN && s != SocketState::SYN_SENT && s != SocketState::SYN_RECEIVED)
    {
        // handle ack
        _send_buffer.handleACK(tcphdr.ack_num, std::chrono::steady_clock::now());
    }

    _send_buffer.setRcvWnd(tcphdr.window_size);

    uint8_t res_flags = 0;

    switch (s)
    {
        case SocketState::LISTEN:
            res_flags = (TCPFlag::SYN | TCPFlag::ACK);
            _peer_addr = src_addr;
            _send_buffer.setPeerAddr(src_addr);
            _recv_buffer.setIRS(tcphdr.seq_num);
            _state = SocketState::SYN_RECEIVED;
            break;
        case SocketState::SYN_SENT:
            _recv_buffer.setIRS(tcphdr.seq_num);
            if (tcphdr.flags == TCPFlag::SYN)
            {
                res_flags = (TCPFlag::SYN | TCPFlag::ACK);
                _state = SocketState::SYN_RECEIVED;
            }
            else //SYN|ACK
            {
                res_flags = TCPFlag::ACK;
                _send_buffer.handleACK(tcphdr.ack_num, std::chrono::steady_clock::now());
                _state = SocketState::ESTABLISHED;
                cv_.notify_all();
            }
            break;
        case SocketState::SYN_RECEIVED:
            if (tcphdr.flags == TCPFlag::ACK)
            {
                _send_buffer.handleACK(tcphdr.ack_num, std::chrono::steady_clock::now());
                _state = SocketState::ESTABLISHED;
                cv_.notify_all();
            }
            else //FIN
            {
                _state = SocketState::LISTEN;
            }
            break;
        case SocketState::ESTABLISHED:
            if (tcphdr.flags == (TCPFlag::FIN | TCPFlag::ACK))
            {
                res_flags = TCPFlag::ACK;
                _state = SocketState::CLOSE_WAIT;
                break;
            }
            // handle normal ack
            if (data_len > 0) res_flags = TCPFlag::ACK;
            break;
        case SocketState::FIN_WAIT_1:
            if (tcphdr.flags == TCPFlag::ACK)
            {
                _state = SocketState::FIN_WAIT_2;
            }
            else if (tcphdr.flags == (TCPFlag::FIN | TCPFlag::ACK))
            {
                res_flags = TCPFlag::ACK;
                _state = TIME_WAIT;
            }
            else //FIN
            {
                res_flags = TCPFlag::ACK;
                _state = SocketState::CLOSING;
            }
            break;
        case SocketState::FIN_WAIT_2:
            if (tcphdr.flags == TCPFlag::ACK)
            {

            }
            else //FIN|ACK
            {
                res_flags = TCPFlag::ACK;
                _state = SocketState::TIME_WAIT;
            }
            break;
        case SocketState::CLOSE_WAIT:
            break;
        case SocketState::CLOSING:
            _state = SocketState::TIME_WAIT;
            break;
        case SocketState::LAST_ACK:
            _state = SocketState::CLOSED;
            break;
        case SocketState::TIME_WAIT:
            break;
        default:
            break;
    }

    return res_flags;

    
    // if (tcphdr.flags == TCPFlag::SYN)
    // {
    //     _peer_addr = src_addr;
    //     _send_buffer.setPeerAddr(src_addr);
    //     _recv_buffer.setIRS(tcphdr.seq_num);
    //     // TODO: check state
    //     {
    //         std::lock_guard lg(m_);
    //         if (_state != SocketState::LISTEN && _state != SocketState::SYN_SENT) return; // TODO: handle error
    //         _state = SocketState::SYN_RECEIVED;
    //         _send_buffer.enqueue(nullptr, 0, TCPFlag::SYN | TCPFlag::ACK);
    //     }
    //     return;
    // }

    // if (tcphdr.flags & TCPFlag::ACK)
    // {
    //     _send_buffer.handleACK(tcphdr.ack_num, std::chrono::steady_clock::now());
    //     std::lock_guard lock(m_);
    //     if (_state == SocketState::SYN_RECEIVED)
    //     {
    //         _state = SocketState::ESTABLISHED;
    //         cv_.notify_all();
    //     }
    // }

    // if (tcphdr.flags & TCPFlag::SYN)
    // {
    //     _recv_buffer.setIRS(tcphdr.seq_num);
    // }

    // //TODO: make sure FIN works
    // if (tcphdr.flags & TCPFlag::SYN || tcphdr.flags & TCPFlag::PSH || tcphdr.flags & TCPFlag::FIN)
    // {
    //     TCPHeader tmp;
    //     auto p = std::make_shared<TCPSegment>(
    //         reinterpret_cast<const std::byte*>(&tmp),
    //         nullptr,
    //         _send_buffer.getSeqNumber(),
    //         sizeof(TCPHeader),
    //         sizeof(TCPHeader),
    //         TCPFlag::ACK
    //     );
    //     {
    //         std::lock_guard lg(m_);
    //         if (_state == SocketState::SYN_SENT)
    //         {
    //             _state = SocketState::ESTABLISHED;
    //         }
    //     }
    //     _engine.send(p, _local_addr, _peer_addr, _recv_buffer);
    //     //TODO: is the right place to wake up blocked recv calls?
    //     cv_.notify_all();
    // }
    
}

ssize_t StreamSocket::recv(std::byte* buf, size_t len)
{
    std::unique_lock<std::mutex> lock(m_);
    cv_.wait(lock, [this]() {
        return _recv_buffer.availableData();
    });
    return _recv_buffer.dequeue(buf, len);
}



}