#include <boost/thread.hpp>
#include <glog/logging.h>
#include "distributor/worker.h"

int main(int argc, char *argv[]) {
    google::InitGoogleLogging(argv[0]);
    using namespace ddc::distributor;
    Worker w;
    w.start();
    w.join();
}
