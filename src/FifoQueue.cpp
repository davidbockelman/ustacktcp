#include <cstdint>
#include <vector>
#include <deque>


class FifoQueue {
    private:

    struct Segment
    {
        uint32_t _seg_start;
        std::vector<unsigned char> _data;

        template<typename T>
        Segment(uint32_t seg_start, T buf, T buf_end) :
        _seg_start(_seg_start)
        {
            _data = std::vector<unsigned char>(buf, buf_end);
        }

        uint32_t nextSegStart() const { return _seg_start + _data.size(); }

        uint32_t getSize() const { return _data.size(); }

        auto at(uint32_t i) const
        {
            return _data.begin() + (i - _seg_start);
        }

        auto end() const { return _data.end(); }
    };

    std::deque<Segment> _q;
    uint32_t _sz;

    public:
        FifoQueue() = default;

        void push(const unsigned char* buf, uint32_t len)
        {
            uint32_t seg_start = 0;
            if (!_q.empty()) seg_start = _q.back().nextSegStart();
            Segment new_seg(seg_start, buf, buf + len);
            _sz += new_seg.getSize();
            _q.emplace_back(std::move(new_seg));
        }

        void trim(uint32_t ack)
        {
            while (!_q.empty() && _q.front().nextSegStart()-1 < ack) 
            {
                _sz -= _q.front().getSize();
                _q.pop_front();
            }
            if (_q.empty()) return; 
            Segment& split = _q.front();
            if (split.nextSegStart() == ack + 1)
            {
                _sz -= split.getSize();
                _q.pop_front();
                return;
            }
            Segment new_seg(ack+1, ++split.at(ack), split.end());
            _sz -= (split.getSize() - new_seg.getSize());
            _q.pop_back();
            _q.emplace_front(std::move(new_seg));
        }



};