
#include "worker.h"
#include <glog/logging.h>
#include <zmq.hpp>
#include "distributor.pb.h"
#include "zmqutils.h"

namespace ddc {
namespace distributor {

void Worker::run() {
    LOG(INFO) << "starting worker";
    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_DEALER);

    std::string id = s_set_id(socket);

    socket.setsockopt(ZMQ_IDENTITY, id.c_str(), id.length());

    socket.connect("tcp://localhost:5671");

    /**
     * send registration
     */
    AnyRequest rsp;
    rsp.set_type(AnyRequest_Type_REGISTRATION);
    Registration *r = new Registration;
    r->set_id(id);
    rsp.set_allocated_registration(r);
    std::string responseStr = rsp.SerializeAsString();
    s_sendmore(socket, "");
    s_send(socket, responseStr);
    LOG(INFO) << "sending registration as " << id;


    while(1) {
        s_recv(socket);     //  Envelope delimiter
        std::string request = s_recv(socket);
        AnyRequest req;
        req.ParseFromString(request);
        AnyRequest_Type type = req.type();
        if(type == AnyRequest_Type_FETCH_SPLIT_REQUEST) {
            AnyRequest rsp;
            rsp.set_type(AnyRequest_Type_FETCH_SPLIT_RESPONSE);
            FetchSplitResponse *r = new FetchSplitResponse;
            r->set_status(0);
            r->set_filename(req.fetchsplitrequest().filename());
            rsp.set_allocated_fetchsplitresponse(r);
            std::string responseStr = rsp.SerializeAsString();
            s_sleep(within(1000));
            s_sendmore(socket, "");
            s_send(socket, responseStr);
            LOG(INFO) << "sending splitResponse for split " << req.fetchsplitrequest().filename();
        }
        else if(type == AnyRequest_Type_HEARTBEAT_REQUEST) {
            AnyRequest rsp;
            rsp.set_type(AnyRequest_Type_HEARTBEAT_RESPONSE);
            HeartBeatResponse *r = new HeartBeatResponse;
            rsp.set_allocated_heartbeatresponse(r);
            std::string responseStr = rsp.SerializeAsString();

            s_sendmore(socket, "");
            s_send(socket, responseStr);
            LOG(INFO) << "sending heartBeatResponse";
        }
        else if(type == AnyRequest_Type_SHUTDOWN_REQUEST) {
            LOG(INFO) << "worker exiting ...";
            return;
        }
        else {
            LOG(ERROR) << "received unknown request";
        }
        boost::this_thread::interruption_point();
    }
}

} // namespace distributor
} // namespace ddc
