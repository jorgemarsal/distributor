#ifndef BASE_RUNNABLE_H
#define BASE_RUNNABLE_H

#include <boost/thread.hpp>

namespace base {

class Runnable {
public:
    virtual ~Runnable() {

    }

    void start() {
        thread_ = boost::thread(&Runnable::run, this/*, arg*/);
    }

    void cancel() {
        thread_.interrupt();
    }

    void join() {
        thread_.join();
    }

    virtual void run() = 0;
private:
    boost::thread thread_;
};

} // namespace base

#endif // BASE_RUNNABLE_H
