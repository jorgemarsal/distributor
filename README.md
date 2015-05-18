Simple example of a master process that sends tasks to a group of workers. They communicate using `protobuf` messages over a `zeromq` transport.

Dependencies: `boost`, `protobuf`, `zeromq`, `glog`.

To compile and run:

    $ make
    $ ./master_test <num_requests>
    $ ./worker_test
    $ ./worker_test  # can start multiple workers
