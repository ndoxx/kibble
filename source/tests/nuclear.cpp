#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"

#include <vector>

using namespace kb;

void init_logger()
{
    KLOGGER_START();

    KLOGGER(create_channel("nuclear", 3));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

// TODO: iterate over unordered pairs, and const iterator
template <typename T>
class PairIterator
{
public:
    PairIterator(std::vector<T> &vec) : vec_(vec)
    {
    }

    static inline PairIterator end(std::vector<T> &vec)
    {
        return PairIterator(vec, vec.size() - 1, vec.size() - 1);
    }

    inline std::pair<T &, T &> operator*() const
    {
        return {vec_[ii_], vec_[jj_]};
    }

    friend inline bool operator==(const PairIterator &it1, const PairIterator &it2)
    {
        return it1.idx() == it2.idx();
    }

    friend inline bool operator!=(const PairIterator &it1, const PairIterator &it2)
    {
        return it1.idx() != it2.idx();
    }

    friend inline bool operator<=(const PairIterator &it1, const PairIterator &it2)
    {
        return it1.idx() <= it2.idx();
    }

    friend inline bool operator>=(const PairIterator &it1, const PairIterator &it2)
    {
        return it1.idx() >= it2.idx();
    }

    friend inline bool operator<(const PairIterator &it1, const PairIterator &it2)
    {
        return it1.idx() < it2.idx();
    }

    friend inline bool operator>(const PairIterator &it1, const PairIterator &it2)
    {
        return it1.idx() > it2.idx();
    }

    inline PairIterator& operator++()
    {
        jj_ = jj_ + 1 < vec_.size() ? jj_ + 1 : 0;
        ii_ = jj_ == 0 ? ii_ + 1 : ii_;
        return *this;
    }

    inline PairIterator operator++(int)
    {
        PairIterator tmp = *this;
        ++(*this);
        return tmp;
    }

private:
    PairIterator(std::vector<T> &vec, size_t ii, size_t jj) : vec_(vec), ii_(ii), jj_(jj)
    {
    }

    inline size_t idx() const
    {
        return ii_ * vec_.size() + jj_;
    }

private:
    std::vector<T> &vec_;
    size_t ii_ = 0;
    size_t jj_ = 0;
};

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    init_logger();

    std::vector<int> myvec{0, 1, 2, 3, 4, 5};

    for (PairIterator it(myvec); it < PairIterator<int>::end(myvec); ++it)
    {
        KLOG("nuclear",1) << (*it).first << ' ' << (*it).second << std::endl; 
    }

    return 0;
}
