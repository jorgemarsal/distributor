#include <map>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <glog/logging.h>
#include <zmq.hpp>
#include "distributor/zmqutils.h"

#include "base/producerconsumerqueue.h"
#include "base/runnable.h"
#include "distributor/distributor.pb.h"
#include "distributor/master.h"
#include "distributor/split.h"
#include "distributor/transport.h"

int main(int argc, char*argv[]) {
    google::InitGoogleLogging(argv[0]);
    using namespace ddc::distributor;
    base::ProducerConsumerQueue<base::Block<FullRequest> > requestQueue;
    base::ProducerConsumerQueue<base::Block<FullRequest> > responseQueue;
    // allow N requests and responses in flight
    // Set queue size keeping in mind the processing time of the worker and the hearbeat timeout.
    // Otherwise we can queue up a lot of stuff for one worker and if another worker joins,
    // the heartbeat will time out. Increase hearbeat timeout for bigger queue sizes.
    requestQueue.configure(5);
    responseQueue.configure(5);
    uint64_t numRequests = 1;
    if(argc == 2) {
        numRequests = atoi(argv[1]);
        assert(numRequests < 1000000);
    }
    Master m(&requestQueue, &responseQueue, numRequests);
    Transport t(&requestQueue, &responseQueue);

    m.start();
    t.start();

    //wait until master is done
    m.join();

    // at this point cancel transport
    t.cancel();
    t.join();

    return 0;
}
