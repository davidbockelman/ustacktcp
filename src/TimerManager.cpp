#include <thread>
#include <iostream>
#include <chrono>

#include <TimerManager.hpp>
#include <StreamSocket.hpp>
#include <types.hpp>

namespace ustacktcp {

void TimerManager::insertSocket(const std::shared_ptr<StreamSocket>& sock)
{
    sock_.push_back(sock);
}

void TimerManager::timeoutLoop()
{
    while (true)
    {
        const auto now = std::chrono::steady_clock::now();
        for (const auto p : sock_)
        {
            SocketState s;
            {
                std::lock_guard lock(p->m_);
                s = p->_state;
            }
            if (s != SocketState::CLOSED && p->_send_buffer.getRTOExpiry() < now)
            {
                p->_send_buffer.handleRTO();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}


}