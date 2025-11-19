#pragma once

#include <vector>
#include <memory>

namespace ustacktcp {

class StreamSocket;
    
class TimerManager {
    private:

    std::vector<std::shared_ptr<StreamSocket>> sock_;

    public:

    void insertSocket(const std::shared_ptr<StreamSocket>&);

    void timeoutLoop();
};

}